/*
 * tcpha_fe_client_connection.c
 *
 *  Created on: Apr 5, 2011
 *      Author: rfliam200
 */

#include "tcpha_fe_client_connection.h"
#include "tcpha_fe_poll.h"
#include "tcpha_fe_socket_functions.h"

kmem_cache_t *tcpha_fe_conn_cachep;
static LIST_HEAD(connection_herders);
static rwlock_t conn_herders_rwlock; /* This is for future "what if" cases,
										currently herders are only
										created or deleted by a single thread */
static int num_pools;
/* This cache is for allocating work to do work_structs */
kmem_cache_t *work_struct_cachep;

/* Private Functions */
/*---------------------------------------------------------------------------*/
static inline void destroy_connection_herders(void);
static int tcpha_fe_herder_run(void *herder);

/* Initilazers etc. */
/*---------------------------------------------------------------------------*/
static inline struct work_struct *work_alloc(void);
static inline void work_free(struct work_struct *work);

static int herder_init(struct tcpha_fe_herder **herder, int cpu);
static void herder_destroy(struct tcpha_fe_herder *herder);

static inline struct tcpha_fe_herder *herder_alloc(void);
static inline void herder_free(struct tcpha_fe_herder *herder);

/* Function implementations */
/*---------------------------------------------------------------------------*/

/* Constructors and allocaters */
static inline struct tcpha_fe_herder *herder_alloc()
{
	return kmalloc(sizeof(struct tcpha_fe_herder), GFP_KERNEL);
}

int herder_init(struct tcpha_fe_herder **herder, int cpu)
{
	int err;
	struct tcpha_fe_herder *h;
	h = herder_alloc();

	/* Create our epoller */
	err = tcp_epoll_init(&h->eventpoll);
	if(err)
		goto epoll_create_err;
	
	/* Create everything else */
	h->cpu = cpu; /* What cpu this herder will be working on */
	INIT_LIST_HEAD(&h->conn_pool);
	INIT_LIST_HEAD(&h->herder_list);
	atomic_set(&h->pool_size, 0);
	rwlock_init(&h->pool_lock);
	*herder = h;
	return 0;
	
epoll_create_err:
	herder_free(h);
	return err;
}

static inline void herder_free(struct tcpha_fe_herder *herder)
{
	kfree(herder);
}

void herder_destroy(struct tcpha_fe_herder *herder)
{
	struct tcpha_fe_conn *conn, *next;

	/* Cleanup connection pool */
	write_lock(&herder->pool_lock);
	list_for_each_entry_safe(conn, next, &herder->conn_pool, list) {
		tcp_epoll_remove(herder->eventpoll, conn->csock);
		tcpha_fe_conn_destroy(conn);
		printk(KERN_ALERT "Connection destroyed on Pool: %u\n", herder->cpu);
    }
	list_del(&herder->conn_pool);
	write_unlock(&herder->pool_lock);

	/* Cleanup epoll */
	list_del(&herder->herder_list);
	tcp_epoll_destroy(herder->eventpoll);

	herder_free(herder);
}

static inline struct work_struct *work_alloc()
{
	return kmem_cache_alloc(work_struct_cachep, GFP_KERNEL);
}
static inline void work_free(struct work_struct *work)
{
	kmem_cache_free(work_struct_cachep, work);
}

/* External setup */
/*---------------------------------------------------------------------------*/
int init_connections(void)
{
	int cpu;
	int err;
	struct tcpha_fe_herder *herder;

	/* Create our memory cache for connections lists */
	tcpha_fe_conn_cachep = kmem_cache_create("tcpha_fe_conn",
						      sizeof(struct tcpha_fe_conn), 0,
						      SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!tcpha_fe_conn_cachep)
		return -ENOMEM;

	work_struct_cachep = kmem_cache_create("tcpha_work_struct_cache", 
			sizeof(struct work_struct), 
			0,
			SLAB_HWCACHE_ALIGN,
			NULL, NULL);

	if (!work_struct_cachep)
		goto errorWorkAlloc;
	
	rwlock_init(&conn_herders_rwlock);
	write_lock(&conn_herders_rwlock);
	/* Create our connection pools to work from */
	/* One connection pool per processor */
	num_pools = 0;
	for_each_online_cpu(cpu) {
		err = herder_init(&herder, cpu);
		if (err)
			goto errorHerderAlloc;		
		num_pools++;
		list_add(&connection_herders, &herder->herder_list);
		printk(KERN_ALERT "Adding Herder for CPU: %u\n", cpu);
		/* Initialize our work, passing ourself as the data object
		 * (basically the this pointer lol) */
		herder->task = kthread_create(tcpha_fe_herder_run, herder, "TCPHA Herder %u", cpu);
		kthread_bind(herder->task, cpu);
		if (!IS_ERR(herder->task))
			wake_up_process(herder->task);
		else
			goto errHerderProc;
	}
	write_unlock(&conn_herders_rwlock);
	
	return 0;

errHerderProc:
	printk(KERN_ALERT "Error Making Herder Process");
errorHerderAlloc:
	kmem_cache_destroy(work_struct_cachep);
	destroy_connection_herders();
errorWorkAlloc:
	kmem_cache_destroy(tcpha_fe_conn_cachep);

	return -ENOMEM;
}

/* TODO: This code needs error handling */
int tcpha_fe_conn_create(struct socket *sock)
{
	struct tcpha_fe_herder *herder;
	int min_pool_size = MAX_INT;
	int herder_pool_size = 0;
	/* TODO: Supress the warning here or something */
	struct tcpha_fe_herder *least_loaded = NULL;
	/* Setup connection */
	struct tcpha_fe_conn *connection = kmem_cache_alloc(tcpha_fe_conn_cachep, GFP_KERNEL);
	//int err;

	connection->csock = sock;
	INIT_LIST_HEAD(&(connection->list));

	/* search for least loaded pool */
	read_lock(&conn_herders_rwlock);
	list_for_each_entry(herder, &connection_herders, herder_list) {
		/* We are not THAT concered if we end up sending to a
		 * slightly more loaded pool, so no need to lock the pool
		 * just use atomic operations */
		herder_pool_size = atomic_read(&herder->pool_size);
		if (herder_pool_size < min_pool_size) {
			min_pool_size = herder_pool_size;
			least_loaded = herder;
		}
	}
	read_unlock(&conn_herders_rwlock);

	/* Now we lock the pool, add the connection
	 * to the poll in and make sure to increase
	 * our pool count! */
	if (!least_loaded) {
		write_lock(&least_loaded->pool_lock);
		list_add(&connection->list, &least_loaded->conn_pool);
		atomic_inc(&least_loaded->pool_size);
		write_unlock(&least_loaded->pool_lock);
		printk(KERN_ALERT "Connection Created on Pool: %u\n", least_loaded->cpu);
	}

	/* And now add it to our epoll interface */
	tcp_epoll_insert(least_loaded->eventpoll, connection->csock, POLLIN);

	return 0;
}

/* Tear down functions for the entire mess */
void tcpha_fe_conn_destroy(struct tcpha_fe_conn* conn)
{
	sock_release(conn->csock);
	conn->csock = NULL;
	list_del(&conn->list);
	kmem_cache_free(tcpha_fe_conn_cachep, conn);
}

static inline void destroy_connection_herders(void)
{	
	struct tcpha_fe_herder *herder, *next;
	write_lock(&conn_herders_rwlock);
	list_for_each_entry_safe(herder, next, &connection_herders, herder_list) {
		kthread_stop(herder->task);
		herder_destroy(herder);
    }
	write_unlock(&conn_herders_rwlock);
}

int destroy_connections(void)
{
	int err;

	destroy_connection_herders();

	err = kmem_cache_destroy(tcpha_fe_conn_cachep);
	err |= kmem_cache_destroy(work_struct_cachep);
	return err;
}

/* This is function responsible for maintaing our connection
 * pools, polling the open connections, and scheduling work to be
 * done on connections when apropriate.
 * They each work for a separate connection pool (to help lower
 * lock contention). It is the main run loops for our connection herder.
 */
static int tcpha_fe_herder_run(void *data)
{
	struct tcpha_fe_herder *herder = (struct tcpha_fe_herder*)data;

	/* While we are still running */
	set_current_state(TASK_INTERRUPTIBLE);
	while(!kthread_should_stop()) {
		if (signal_pending(current)) {
		}
		/* Poll our open connections */ 
		read_lock(&herder->pool_lock);
		read_unlock(&herder->pool_lock);

		/* Schedule ones with stuff to do for processing */

		/* Delete dead ones */
		
		set_current_state(TASK_INTERRUPTIBLE);
	}
	set_current_state(TASK_RUNNING);

	return 0;
}
