/*
 * tcpha_fe_client_connection.c
 *
 *  Created on: Apr 5, 2011
 *      Author: rfliam200
 */

#include "tcpha_fe_client_connection.h"

kmem_cache_t *tcpha_fe_conn_cachep;
static LIST_HEAD(connection_herders);
static rwlock_t conn_herders_rwlock;
static struct workqueue_struct *conn_herders_wq;
static int num_pools;

void init_connection_herder(struct tcpha_fe_herder *herder, int cpu)
{
	herder->cpu = cpu;
	INIT_LIST_HEAD(&herder->conn_pool);
	INIT_LIST_HEAD(&herder->herder_list);
	atomic_set(&herder->pool_size, 0);
	rwlock_init(&herder->pool_lock);
	list_add(&herder->herder_list, &connection_herders);
}

int init_connections(void)
{
	int cpu;

	/* Create our memory cache for connections lists */
	tcpha_fe_conn_cachep = kmem_cache_create("tcpha_fe_conn",
						      sizeof(struct tcpha_fe_conn), 0,
						      SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!tcpha_fe_conn_cachep) {
		return -1;
	}

	rwlock_init(&conn_herders_rwlock);
	write_lock(&conn_herders_rwlock);
	/* Create our connection pools to work from */
	/* One connection pool per processor */
	num_pools = 0;
	for_each_online_cpu(cpu) {
		struct tcpha_fe_herder *herder = kmalloc(sizeof(struct tcpha_fe_herder), GFP_KERNEL);
		if (herder) {
			num_pools++;
			init_connection_herder(herder, cpu);
		} else {
			return - 1;
		}
	}
	write_unlock(&conn_herders_rwlock);

	/* Create our work queues */

	return 0;
}

int tcpha_fe_conn_create(struct socket *sock)
{
	struct tcpha_fe_herder *herder;
	int min_pool_size = MAX_INT;
	int herder_pool_size = 0;
	struct tcpha_fe_herder *least_loaded;
	/* Setup connection */
	struct tcpha_fe_conn *connection = kmem_cache_alloc(tcpha_fe_conn_cachep, GFP_KERNEL);
	//int err;

	connection->csock = sock;
	INIT_LIST_HEAD(&(connection->list));

	/* add it to a connection pool */
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
	write_lock(&least_loaded->pool_lock);
	list_add(&connection->list, &least_loaded->conn_pool);
	atomic_inc(&least_loaded->pool_size);
	write_unlock(&least_loaded->pool_lock);

	return 0;
}

void tcpha_fe_conn_destroy(struct tcpha_fe_conn* conn)
{
		sock_release(conn->csock);
		conn->csock = NULL;
		list_del(&conn->list);
		kmem_cache_free(tcpha_fe_conn_cachep, conn);
}

void destroy_connection_herder(struct tcpha_fe_herder *herder)
{
	struct tcpha_fe_conn *conn, *next;

	write_lock(&herder->pool_lock);
	list_for_each_entry_safe(conn, next, &herder->conn_pool, list) {
		tcpha_fe_conn_destroy(conn);
    }
	list_del(&herder->conn_pool);
	write_unlock(&herder->pool_lock);
	list_del(&herder->herder_list);
}

int destroy_connections(void)
{
	struct tcpha_fe_herder *herder, *next;

	write_lock(&conn_herders_rwlock);
	list_for_each_entry_safe(herder, next, &connection_herders, herder_list) {
		destroy_connection_herder(herder);
    }
	write_unlock(&conn_herders_rwlock);

	return kmem_cache_destroy(tcpha_fe_conn_cachep);
}
