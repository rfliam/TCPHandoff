#include "tcpha_be_fe_connection.h"
#include "tcpha_be_handoff_connection.h"
#include "tcpha_be.h"
#include "../frontend/tcpha_fe_socket_functions.h"

int tcphafe_max_backlog = 2048;
int main_sleep_time = 1 * HZ;

/* Private Function Prototypes */
/*---------------------------------------------------------------------------*/
int setup_server_socket(struct tcpha_be_server *server);
void pull_down_server_socket(struct tcpha_be_server *server);
static struct tcpha_be_handoff_connection *choose_connection(struct tcpha_be_fe_connection *conn);
static void parse_message(struct tcpha_be_fe_connection *conn, int len);

/* Alloc/Free Function Prototypes */
/*---------------------------------------------------------------------------*/
static inline void be_fe_conn_alloc(struct tcpha_be_fe_connection **conn);
static void be_fe_conn_init(struct tcpha_be_fe_connection **conn);
static void be_fe_conn_free(struct tcpha_be_fe_connection *conn);

/* Public Function Prototypes */
/*---------------------------------------------------------------------------*/
int tcpha_be_server_daemon(void * __service)
{
	/* Variables for dealing with server */
	struct tcpha_be_server *server = (struct tcpha_be_server*)__service;
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
		/* List for accepts on the main socket */
		err = kernel_accept(server->listener, &newsock, O_NONBLOCK);
		if (err < 0) {
			schedule_timeout_interruptible(main_sleep_time);
		} else {
			err = establish_be_fe_connection(server, newsock);
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

/* TODO: This should become a common method for both probably v0v */
/**
 * Setup the listening socket for the main accept thread.
 */
int setup_server_socket(struct tcpha_be_server *server)
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
    /* TODO: Limit this to connections coming from front end */
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = (__force u16)htons(server->lport);

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

	server->listener = sock;
	return 0;
}

/*
 * Destroy the listening socket for the main accept thread.
 */
void pull_down_server_socket(struct tcpha_be_server *server)
{
	sock_release(server->listener);
}

extern int establish_be_fe_connection(struct tcpha_be_server *server, struct socket *sock)
{
    struct tcpha_be_fe_connection *conn;
    be_fe_conn_init(&conn);
    conn->sock = sock;

    /* Add the connection to the list */
    spin_lock(&server->fe_connections_lock);
    server->num_fe_connections++;
    list_add(&server->fe_connections_list, &conn->list);
    spin_unlock(&server->fe_connections_lock);

    /* Start a thread to listen for incoming data */
    conn->thread = kthread_run(be_fe_connection_daemon, conn, "FE Connection %d", server->num_fe_connections);
    
    return 0;
}

int be_fe_connection_daemon(void *__service)
{
    struct tcpha_be_fe_connection *conn = __service;
    int len;
    struct kvec vec;
    struct msghdr msg;
    wait_queue_t wait;

    init_waitqueue_entry(&wait, current);

    set_current_state(TASK_INTERRUPTIBLE);
    while (!kthread_should_stop()) {
	    msg.msg_control = NULL;
	    msg.msg_controllen = 0;

	    if (conn->num_read == 0) {
	        vec.iov_base = &conn->buffer[0];
	    } else {
	        vec.iov_base = &conn->buffer[conn->num_read];
	    }

	    /* Hook into the socket poll list and sleep on it */
    	add_wait_queue(conn->sock->sk->sk_sleep, &wait);
    	/* Process data when we are woken */
	    len = kernel_recvmsg(conn->sock, &msg, &vec, 1, MAX_BUFFER_SIZE, MSG_DONTWAIT);
	    /* Determine who the data is for */
	    parse_message(conn, len);
	    /* Process it */
       set_current_state(TASK_INTERRUPTIBLE);
    }
    __set_current_state(TASK_RUNNING);

    /* We are shutting down inform the front end if necessary... */

    /* Free up any unused resources */
    sock_release(conn->sock);

    return 0;
}


static void parse_message(struct tcpha_be_fe_connection *conn, int len)
{
    conn->num_read += len;
    if (conn->num_read >= sizeof(struct tcpha_ipv4_hdr) + sizeof(struct tcpha_hdr)) {
	    conn->ipv4hdr.ipaddress = 0;
       	conn->ipv4hdr.port = 0;

	    /* Format specifies first byte is the "what to do" mask */
	    conn->hdr.cmd = conn->buffer[0];

    	/* Next byte is the ip version */
    	conn->hdr.ipversion = conn->buffer[1];

	    /* Source Ip address */
    	conn->ipv4hdr.ipaddress = (conn->ipv4hdr.ipaddress << 8) + conn->buffer[5];
	    conn->ipv4hdr.ipaddress = (conn->ipv4hdr.ipaddress << 8) + conn->buffer[4];
    	conn->ipv4hdr.ipaddress = (conn->ipv4hdr.ipaddress << 8) + conn->buffer[3];
	    conn->ipv4hdr.ipaddress = (conn->ipv4hdr.ipaddress << 8) + conn->buffer[2];

    	/* Source Port */
	    conn->ipv4hdr.port = (conn->ipv4hdr.port << 8) + conn->buffer[8];
    	conn->ipv4hdr.port = (conn->ipv4hdr.port << 8) + conn->buffer[7];

    	/* Len */
	    conn->ipv4hdr.len = (conn->ipv4hdr.len << 8) + conn->buffer[10];
    	conn->ipv4hdr.len = (conn->ipv4hdr.len << 8) + conn->buffer[9];
    	
    	/* Execute the Command */
	    /* TODO: handling for failed processing */
    	process_data_for_connection(conn);
    }
}

extern int stop_fe_connections(struct tcpha_be_server *server)
{
    int errs = 0;
    struct tcpha_be_fe_connection *conn, *next;
    /* Find each server and kill it. */
    list_for_each_entry_safe(conn, next, &server->fe_connections_list, list) {
	    errs |= kthread_stop(conn->thread);
	    list_del(&conn->list);
    }

    return errs;
}
/* Lifecycle Methods */
/*---------------------------------------------------------------------------*/
static inline void be_fe_conn_alloc(struct tcpha_be_fe_connection **conn)
{
	*conn = kzalloc(sizeof(struct tcpha_be_fe_connection), GFP_KERNEL);
}

static void be_fe_conn_init(struct tcpha_be_fe_connection **conn)
{
    be_fe_conn_alloc(conn);
    INIT_LIST_HEAD(&(*conn)->list);
    INIT_LIST_HEAD(&(*conn)->handoff_conn_list);
}

static void be_fe_conn_free(struct tcpha_be_fe_connection *conn)
{
    kfree(conn);
}

