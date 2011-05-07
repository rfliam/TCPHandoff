/*
 * tcpha_fe_server.c
 *
 *  Created on: Mar 24, 2011
 *      Author: rfliam200
 */

#include "tcpha_fe_server.h"
#include "tcpha_fe_client_connection.h"
#include "tcpha_fe_socket_functions.h"

int tcphafe_max_backlog = 2048;
int main_sleep_time = 1 * HZ;

/**
 * The fe_server_daemon is responsible for setting up and
 * maintaining the worker daemons, and dealing with the accepts
 * on the listening socket. It actively polls (instead of being interrupt driven)
 * on the main socket.
 */
int tcpha_fe_server_daemon(void * __service)
{
	/* Variables for dealing with server */
	struct tcpha_fe_server *server = (struct tcpha_fe_server*)__service;
	struct socket *newsock;
	int err;
	printk(KERN_ALERT "Server Starting Up\n");

	/* Setup our server socket */
	err = setup_server_socket(server);
	if (err < 0)
		goto server_setup_fail;

	/* We are go */
	atomic_set(&server->running, 1);
	while (!kthread_should_stop()) {
		if (signal_pending(current)) {
			/* TODO: Change to correct code */
			//do_exit(0);
		}
		/* List for accepts on the main socket */
		err = kernel_accept(server->mainsock, &newsock, O_NONBLOCK);
		if (err < 0) {
			schedule_timeout_interruptible(main_sleep_time);
		} else {
			err = tcpha_fe_conn_create(server->herders, newsock);
			if (err < 0)
				goto connection_err;

			printk(KERN_ALERT "Connection made\n");
			continue;
connection_err:
			sock_release(newsock);
			continue;
		}
	}

	/* We are done */
	printk(KERN_ALERT "Server Shutting Down\n");
	pull_down_server_socket(server);
	atomic_set(&server->running, 0);
	return 0;

server_setup_fail:
	printk(KERN_ALERT "Server Failed to Initialize\n");
	return -1;	
}

/**
 * Setup the listening socket for the main accept thread.
 */
int setup_server_socket(struct tcpha_fe_server *server)
{
	struct socket *sock;
	struct sockaddr_in sin;
	int error;
	printk(KERN_ALERT "Creating Main Socket\n");

	/* First create a socket */
	error = sock_create_kern(PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
	if (error) {
		printk(KERN_ALERT "Error during creation of socket; terminating\n");
		return error;
	}
	printk(KERN_ALERT "Binding Main Socket\n");
	/* Now bind the socket */
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = (__force u16)htons(server->conf.port);

	/* set the option to reuse the address. */
	sock->sk->sk_reuse = 1;

	/* Bind the socket to the correct address */
	error = kernel_bind(sock, (struct sockaddr *) &sin, sizeof(sin));
	if (error < 0) {
		printk(KERN_ERR "Error binding socket. This means that other daemon is (or was a short time ago).");
		return error;
	}

	printk(KERN_ALERT "Listening on Main Socket\n");
	/* Now, start listening on the socket */
	error = kernel_listen(sock, tcphafe_max_backlog);
	if (error != 0) {
		printk(KERN_ERR "Error listening on socket \n");
		return error;
	}

	server->mainsock = sock;
	return 0;
}

/*
 * Destroy the listening socket for the main accept thread.
 */
void pull_down_server_socket(struct tcpha_fe_server *server)
{
	sock_release(server->mainsock);
}

