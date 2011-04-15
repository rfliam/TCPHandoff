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
#include <linux/sched.h>

extern kmem_cache_t *tcpha_fe_conn_cachep;

#define MAX_INT 0x7ffffff

/* A connection with client */
struct tcpha_fe_conn {
	rwlock_t lock;
	struct socket *csock;	/* socket connected to client */
	struct list_head list;	/* d-linked list head */
};

struct tcpha_fe_herder {
	struct list_head conn_pool; /* A pool of connections for us to maintain */
	rwlock_t pool_lock; /* Lock for the connection pool */
	atomic_t pool_size; /* Number of connections currently in pool */
	int cpu; /* The cpu this herders is bound to */
	struct list_head herder_list; /* This is for the list of herders */
};

extern int init_connections(void);

extern int tcpha_fe_conn_create(struct socket *sock);
extern void tcpha_fe_conn_destroy(struct tcpha_fe_conn* conn);
extern int destroy_connections(void);

void init_connection_herder(struct tcpha_fe_herder *herder, int cpu);
void destroy_connection_herder(struct tcpha_fe_herder *herder);
#endif /* TCPHA_FE_CLIENT_CONNECTION_H_ */
