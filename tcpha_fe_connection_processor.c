#include "tcpha_fe_connection_processor.h"
#include "tcpha_fe_client_connection.h"
#include "tcpha_fe_socket_functions.h"

struct kmem_cache *event_process_memcache_ptr;

/* Constructore/destructor methods */
/*---------------------------------------------------------------------------*/

int processor_init(struct workqueue_struct **processor)
{
	*processor = create_workqueue("TCPHA_Connection_Processor");
	event_process_memcache_ptr = kmem_cache_create("TCPHA_Event_Process", 
												sizeof(struct event_process),
													0, SLAB_HWCACHE_ALIGN | SLAB_PANIC,
													NULL, NULL);
	return 0;
}

void processor_destroy(struct workqueue_struct *processor)
{
	flush_workqueue(processor);
	destroy_workqueue(processor);
	kmem_cache_destroy(event_process_memcache_ptr);
}


void event_process_alloc(struct event_process** ep)
{
	*ep = kmem_cache_alloc(event_process_memcache_ptr, GFP_KERNEL);
}
void event_process_free(struct event_process* ep)
{
	kmem_cache_free(event_process_memcache_ptr, ep);
}


/* External (public) methods */
/*---------------------------------------------------------------------------*/
void process_connection(void * data)
{
	struct kvec vec;
	struct msghdr msg;
	int len;
	char buffer[1024];
	struct event_process *ep = data;
	struct tcpha_fe_conn *conn = ep->conn;
	unsigned int events = ep->events;
	struct inet_sock *sk = inet_sk(conn->csock->sk);

	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	vec.iov_base = &buffer;
	vec.iov_len = 1024;

	printk(KERN_ALERT "Work done on connection %u.%u.%u.%u\n", NIPQUAD(sk->daddr));
	/*int kernel_recvmsg(struct socket *sock, struct msghdr *msg, 
                    struct kvec *vec, size_t num,
                    size_t size, int flags);*/

	/* Run throught he events to process */
	if (events & POLLIN) {
		len = kernel_recvmsg(conn->csock, &msg, &vec, 1, 1024, MSG_DONTWAIT);
		if (len > 0 && len < 1023) {
			buffer[len + 1] = '\0';
			printk(KERN_ALERT "Got String: %s\n", buffer);
		}
		if (len == EAGAIN) 
			printk(KERN_ALERT "EAGAIN Eror\n");
	}

	/* Remove the socket from the list */
	if (events & POLLHUP) {
	}

	/* We are done processing them, free the item we where processing */
	event_process_free(ep);
}
