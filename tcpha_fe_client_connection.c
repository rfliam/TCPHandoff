/*
 * tcpha_fe_client_connection.c
 *
 *  Created on: Apr 5, 2011
 *      Author: rfliam200
 */

#include "tcpha_fe_client_connection.h"
#include "tcpha_fe_poll.h"
#include "tcpha_fe_socket_functions.h"

kmem_cache_t *tcpha_fe_conn_cachep = NULL;
atomic_t mem_cache_use = ATOMIC_INIT(0);
static int num_pools;
/* This cache is for allocating work to do work_structs */
kmem_cache_t *work_struct_cachep = NULL;

/* Private Functions */
/*---------------------------------------------------------------------------*/
static void destroy_connection_herders(struct herder_list *herders);
static int tcpha_fe_herder_run(void *herder);

/* Initilazers etc. */
/*---------------------------------------------------------------------------*/
static inline struct work_struct *work_alloc(void);
static inline void work_free(struct work_struct *work);

static int herder_init(struct tcpha_fe_herder **herder, int cpu);
static void herder_destroy(struct tcpha_fe_herder *herder);

static inline struct tcpha_fe_herder *herder_alloc(void);
static inline void herder_free(struct tcpha_fe_herder *herder);

static inline void herder_list_init(struct herder_list *herders);

static inline void tcpha_fe_conn_destroy(struct tcpha_fe_conn* conn);
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

static inline void herder_list_init(struct herder_list *herders)
{
	INIT_LIST_HEAD(&herders->list);
	rwlock_init(&herders->lock);
}
/* Externaly Available Functions */
/*---------------------------------------------------------------------------*/
/*
 * Herder should be already alloced, but no need to init it (i will do that). 
 */
int init_connections(struct herder_list *herders)
{
	int cpu;
	int err;
	struct tcpha_fe_herder *herder;

	atomic_inc(&mem_cache_use);
	/* Create our memory caches if they don't already exist */
	if (tcpha_fe_conn_cachep == NULL)
		tcpha_fe_conn_cachep = kmem_cache_create("tcpha_fe_conn",
								  sizeof(struct tcpha_fe_conn), 0,
								  SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!tcpha_fe_conn_cachep)
		return -ENOMEM;

	if (work_struct_cachep == NULL)
		work_struct_cachep = kmem_cache_create("tcpha_work_struct_cache", 
				sizeof(struct work_struct), 
				0,
				SLAB_HWCACHE_ALIGN,
				NULL, NULL);

	if (!work_struct_cachep)
		goto errorWorkAlloc;

	herder_list_init(herders);
	write_lock(&herders->lock);
	/* Create our connection pools to work from */
	/* One connection pool per processor */
	num_pools = 0;
	for_each_online_cpu(cpu) {
		err = herder_init(&herder, cpu);
		if (err)
			goto errorHerderAlloc;
	
		list_add(&herder->herder_list, &herders->list);
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
	write_unlock(&herders->lock);
	
	return 0;

errHerderProc:
	printk(KERN_ALERT "Error Making Herder Process");
errorHerderAlloc:
	destroy_connection_herders(herders);
	kmem_cache_destroy(work_struct_cachep);
errorWorkAlloc:
	kmem_cache_destroy(tcpha_fe_conn_cachep);
	atomic_dec(&mem_cache_use);
	return -ENOMEM;
}

/* TODO: This code needs error handling */
/*
 * Add a socket connection to the herders to be processed.
 * @herders : The list of herders to chose from
 * @sock : The socket to add
 */
int tcpha_fe_conn_create(struct herder_list *herders, struct socket *sock)
{
	struct tcpha_fe_herder *herder;
	int min_pool_size = MAX_INT;
	int herder_pool_size = 0;
	struct tcpha_fe_herder *least_loaded = NULL;
	/* Setup connection */
	struct tcpha_fe_conn *connection = kmem_cache_alloc(tcpha_fe_conn_cachep,
																  GFP_KERNEL);

	connection->csock = sock;
	INIT_LIST_HEAD(&connection->list);

	/* search for least loaded pool */
	read_lock(&herders->lock);
	list_for_each_entry(herder, &herders->list, herder_list) {
		/* We are not THAT concered if we end up sending to a
		 * slightly more loaded pool, so no need to lock the pool
		 * just use atomic operations */
		herder_pool_size = atomic_read(&herder->pool_size);
		if (herder_pool_size < min_pool_size) {
			min_pool_size = herder_pool_size;
			least_loaded = herder;
		}
	}
	read_unlock(&herders->lock);

	/* Now we lock the pool, add the connection
	 * to the pool in and make sure to increase
	 * our pool count! */
	if (least_loaded) {
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

/* Tear down function */
/*---------------------------------------------------------------------------*/
static inline void tcpha_fe_conn_destroy(struct tcpha_fe_conn* conn)
{
	if (conn->csock)
		sock_release(conn->csock);
	conn->csock = NULL;
	list_del(&conn->list);
	kmem_cache_free(tcpha_fe_conn_cachep, conn);
}

/*
 * Kill a list of connection herders. Kill them dead.
 */
static void destroy_connection_herders(struct herder_list *herders)
{	
	struct tcpha_fe_herder *herder, *next;
	int err = 0;
	printk(KERN_ALERT "Destroying Connections\n");

	/* TODO: Re-write with locking (kthread_stop does not work in an
	 * atomic context !)*/
	list_for_each_entry_safe(herder, next, &herders->list, herder_list) {
		if (!herder) {
			printk(KERN_ALERT "Herder Error\n");
			continue;
		}
		printk(KERN_ALERT "Stoping Herder %u\n", herder->cpu);
		err = kthread_stop(herder->task);
		if (err)
			printk(KERN_ALERT "Error Killing Proc\n");
		/* We need to remove the epoll stuff before killing the connection
		 * other wise we will end up with bad memory access on the socket */
		printk(KERN_ALERT "Destroying Herder\n");
		herder_destroy(herder);
	}
}

int destroy_connections(struct herder_list *herders)
{
	int err = 0;
	destroy_connection_herders(herders);

	if (atomic_dec_and_test(&mem_cache_use)) {
		err = kmem_cache_destroy(tcpha_fe_conn_cachep);
		err |= kmem_cache_destroy(work_struct_cachep);
	}
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
	int maxevents = 1024;
	struct socket *socks[maxevents];
	int numevents;

	printk(KERN_ALERT "Running Herder %u\n", herder->cpu);

	set_current_state(TASK_INTERRUPTIBLE);
	while (!kthread_should_stop()) {
		err = tcp_epoll_wait(herder->eventpoll, socks, maxevents);
		set_current_state(TASK_INTERRUPTIBLE);
		if (numevents == 0 && kthread_should_stop()) {
			break;
		}
	}
	set_current_state(TASK_RUNNING);

	printk(KERN_ALERT "Herder Shutting Down\n");

	return 0;
}
