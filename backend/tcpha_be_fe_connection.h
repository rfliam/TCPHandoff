#ifndef _TCPHA_BE_FE_CONNECTION_H_
#define _TCPHA_BE_FE_CONNECTION_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/net.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <linux/tcp.h>
#include <linux/wait.h>

#define MAX_BUFFER_SIZE 2048

/* TODO: Refactor to common */
struct tcpha_hdr {
	u8 cmd;
	u8 ipversion;
};

/* TODO: Ipv6 support... */
struct tcpha_ipv4_hdr {
	u32 ipaddress;
	u16 port;
	u16 len;
};

/**
 * This structure represents a connection 
 * with a front end server overwhich incoming connections or 
 * packets are relayed.
 * 
 * @author rfliam200 (6/3/2011)
 */
struct tcpha_be_fe_connection {
	struct tcpha_hdr hdr;
	struct tcpha_ipv4_hdr ipv4hdr;
    struct socket *sock;
    struct list_head list;
    struct task_struct *thread;
    /* TODO: Needs to become a radix tree...*/
    struct list_head handoff_conn_list;
    char buffer[MAX_BUFFER_SIZE + 1];
    unsigned int num_read;
	
};

struct tcpha_be_server;

/**
 * The listener kernel thread method. Accepts and instantiates 
 * new connections with the front end.
 * 
 * @author rfliam200 (6/7/2011)
 * 
 * @param __service The tcpha_be_server we will be working with.
 * 
 * @return int Necessary for kthread, status code (should always 
 *         be 0).
 */
extern int tcpha_be_server_daemon(void *__service); 

/**
 * Used to bind a new connection from a front end in.
 * 
 * @author rfliam200 (6/7/2011)
 * 
 * @param sock The socket that resulted from accept.
 */
extern int establish_be_fe_connection(struct tcpha_be_server *server, struct socket *sock);

/**
 * The primary thread loop for dealing with data being sent from
 * the frontend to the backend. In particular this is 
 * responsible for instantiating new sockets to be used and 
 * adding data to already exisisting ones. 
 * 
 * @author rfliam200 (6/7/2011)
 * 
 * @param __service The tcpha_be_fe_connection this thread will 
 *                  work on.
 * 
 * @return int Required by kernel thread, status code of method 
 *         result.
 */
extern int be_fe_connection_daemon(void *__service);

/**
 * Stops any running fe connections from the backend. Call this 
 * before kiling the be server daemon. 
 * 
 * @author rfliam200 (6/7/2011)
 * 
 * @param server The server whose fe connections we will kill.
 * 
 * @return int Any errors presented by the dead threads.
 */
extern int stop_fe_connections(struct tcpha_be_server *server);
#endif
