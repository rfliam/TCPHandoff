/*
 * tcpha_fe_client_connection.c
 *
 *  Created on: Apr 5, 2011
 *      Author: rfliam200
 */

#include "tcpha_fe_client_connection.h"

kmem_cache_t *tcpha_fe_conn_cachep;
static LIST_HEAD(connection_list);

int init_connections(void)
{
	/* Create our connection cache */
	tcpha_fe_conn_cachep = kmem_cache_create("tcpha_fe_conn",
						      sizeof(struct tcpha_fe_conn), 0,
						      SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!tcpha_fe_conn_cachep) {
		return -1;
	}

	return 0;
}

struct tcpha_fe_conn* tcpha_fe_conn_create(struct socket *sock)
{
	struct tcpha_fe_conn *connection = kmem_cache_alloc(tcpha_fe_conn_cachep, GFP_KERNEL);

	connection->csock = sock;

	INIT_LIST_HEAD(&(connection->list));
	list_add(&(connection->list), &connection_list);

	return connection;
}

int destroy_connections(void)
{
//	
	//struct tcpha_fe_conn *conn;
	struct list_head* position;
	list_for_each(position, &connection_list) {
//         printk ("surfing the linked list next = %p and prev = %p\n" ,
//             position->next,
//             position->prev );
    }
	return kmem_cache_destroy(tcpha_fe_conn_cachep);
}
