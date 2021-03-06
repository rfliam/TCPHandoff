/**
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
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include "tcpha_fe_http.h"
#define MAX_INT 0x7ffffff
#define TCPHA_EPOLL_SIZE 1024

extern kmem_cache_t *tcpha_fe_conn_cachep;
struct tcp_eventpoll; /* Pre dec so I can use it here */

struct http_request;

/* A connection with client */
/* TODO: This needs to go in a seperate header file, its used infar to many places */
struct tcpha_fe_conn {
	rwlock_t lock;
	unsigned int events;
	struct socket *csock;	/* socket connected to client */
	struct list_head list;	/* d-linked list head */ 
	struct http_request request;
    atomic_t alive;
};

struct herder_list {
	struct list_head list;
	rwlock_t lock;
};

struct tcpha_fe_herder {
	struct list_head conn_pool; /* A pool of connections for us to maintain */
	rwlock_t pool_lock; /* Lock for the connection pool */
	atomic_t pool_size; /* Number of connections currently in pool */

	int cpu; /* The cpu this herders is bound to */
	struct tcp_eventpoll *eventpoll; /* My epoller */
	
	struct list_head herder_list; /* This is for the list of herders */

	struct work_struct work; /* This is my own job item */
	struct workqueue_struct *processor_work; /* This is the job item
												for my processor */

	struct task_struct *task; /* The task this boy is actually running in */
};

extern int init_connections(struct herder_list *herders, struct workqueue_struct *processors);
extern int destroy_connections(struct herder_list *herders);

/**
 * Add a socket to be "watched" for a new connection into the 
 * list of herders. 
 * 
 * @author rfliam200 (5/27/2011)
 * 
 * @param herders The list of herders which will be watching 
 * @param sock 
 * 
 * @return int If less than 0 there was an err adding to the 
 *         herders (try again or drop the connection).
 */
extern int tcpha_fe_conn_create(struct herder_list *herders, struct socket *sock);


/**
 * The main body loop for a herder
 * 
 * @author rfliam200 (5/27/2011)
 * 
 * @param herder A struct tcpha_fe_herder representing this 
 * herder, this is responsible for listening and maintaing 
 * events on the pool of connections. 
 * 
 * @return int If this hereder encounetered an error durring its 
 * run this will return less than 0. 
 */
extern int tcpha_fe_herder_run(void *herder);
extern void tcpha_fe_conn_destroy(struct tcpha_fe_herder *herder, struct tcpha_fe_conn *conn);

#endif /* TCPHA_FE_CLIENT_CONNECTION_H_ */
