/*
 * tcpha_fe_server.c
 *
 *  Created on: Mar 24, 2011
 *      Author: rfliam200
 */

#include "tcpha_fe_server.h"
#include "tcpha_fe_client_connection.h"

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
	struct socket *new_sock;
	struct tcpha_fe_conn *connection;
	int err;
	printk(KERN_ALERT "Server Starting Up\n");

	/* Setup our server socket */
	err = setup_server_socket(server);
	if(err < 0)
		goto server_setup_fail;

	/* Setup the connections pool */
	err = init_connections();
	if(err)
		goto connection_setup_fail;

	/* We are go */
	atomic_set(&server->running, 1);
	while (!kthread_should_stop()) {
		if (signal_pending(current)) {
		}
		/* List for accepts on the main socket */
		err = server->mainsock->ops->accept(server->mainsock, 
													new_sock,
													O_NONBLOCK);
		if (err < 0) {
			schedule_timeout_interruptible(main_sleep_time);
		} else {
			connection = tcpha_fe_conn_create(new_sock);
			printk(KERN_ALERT "Connection made\n");
			err = sock_create_lite(PF_INET, SOCK_STREAM, IPPROTO_TCP, &new_sock);
			printk(KERN_ALERT "Socket Creation Error\n");
		}
	}

	/* We are done */
	printk(KERN_ALERT "Server Shutting Down\n");
	destroy_connections();
	pull_down_server_socket(server);
	atomic_set(&server->running, 0);
	return 0;
connection_setup_fail:
	pull_down_server_socket(server);
server_setup_fail:
	printk(KERN_ALERT "Server Failed to Initialize\n");
	return -1;	
}

/**
 * The fe_worker_daemon deals with a single connection. It is thread per
 * connection and interrupt driven.
 * We simply wait for an interrupt from the socket and add our selves to
 * the work queue.
 */
int tcpha_fe_worker_daemon(struct tcpha_fe_server *server)
{
	return 0;
}

/**
 * The connection processor is the function called by our work queue to
 * do the proper processing.
 */

int setup_server_socket(struct tcpha_fe_server *server)
{
	struct socket *sock;
	struct sockaddr_in sin;
	int error;
	printk(KERN_ALERT "Creating Main Socket\n");

	/* First create a socket */
	error = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
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
	error = sock->ops->bind(sock, (struct sockaddr *) &sin, sizeof(sin));
	if (error < 0) {
		printk(KERN_ERR "Error binding socket. This means that other daemon is (or was a short time ago).");
		return error;
	}

	printk(KERN_ALERT "Listening on Main Socket\n");
	/* Now, start listening on the socket */
	error = sock->ops->listen(sock, tcphafe_max_backlog);
	if (error != 0) {
		printk(KERN_ERR "Error listening on socket \n");
		return error;
	}

	server->mainsock = sock;

	return 0;
}


void pull_down_server_socket(struct tcpha_fe_server *server)
{
	sock_release(server->mainsock);
}

