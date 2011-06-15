#ifndef _TCPHA_BE_HANDOFF_CONNECTION_H_
#define _TCPHA_BE_HANDOFF_CONNECTION_H_

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/kernel.h>
#include <net/inet_connection_sock.h>
#include <net/inet_sock.h>
#include <net/tcp.h>
#include <linux/tcp.h>

/**
 * This structure holds a connection that has been handed off 
 * to a backend. It is used for updating and placing information 
 * on the socket as packets are relayed across. 
 * 
 * @author rfliam200 (6/3/2011)
 */
struct tcpha_be_handoff_connection {
    /* The socket for the connection */
	struct tcp_sock *sock;
	u32 ipaddr;
	u16 port;
	/* Todo elsewhere but should be radix tree */
	struct list_head list;
};

struct tcpha_be_fe_connection;

extern bool process_data_for_connection(struct tcpha_be_fe_connection *conn);

#endif
