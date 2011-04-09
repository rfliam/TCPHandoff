/*
 * tcpha_fe_client_connection.h
 *
 *  Created on: Apr 5, 2011
 *      Author: rfliam200
 */

#ifndef TCPHA_FE_CLIENT_CONNECTION_H_
#define TCPHA_FE_CLIENT_CONNECTION_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <net/sock.h>

extern kmem_cache_t *tcpha_fe_conn_cachep;

/* A connection with client */
struct tcpha_fe_conn {
	rwlock_t lock;
	struct socket *csock;	/* socket connected to client */
	struct list_head list;	/* d-linked list head */
};

extern int init_connections(void);

extern struct tcpha_fe_conn* tcpha_fe_conn_create(struct socket *sock);

extern int destroy_connections(void);

#endif /* TCPHA_FE_CLIENT_CONNECTION_H_ */
