#include "tcpha_fe_connection_processor.h"
#include "tcpha_fe_client_connection.h"
#include "tcpha_fe_socket_functions.h"
#include "tcpha_fe_http.h"

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

	http_init();
	return 0;
}

void processor_destroy(struct workqueue_struct *processor)
{
	flush_workqueue(processor);
	destroy_workqueue(processor);
	kmem_cache_destroy(event_process_memcache_ptr);
	http_destroy();
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
	int hdrlen;
	struct event_process *ep = data;
	struct tcpha_fe_conn *conn = ep->conn;
	unsigned int events = ep->events;
	struct inet_sock *sk = inet_sk(conn->csock->sk);
	char tmp_buffer[MAX_HEADER_SIZE];

	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	

	printk(KERN_ALERT "Work done on connection %u.%u.%u.%u\n", NIPQUAD(sk->daddr));
	/*int kernel_recvmsg(struct socket *sock, struct msghdr *msg, 
                    struct kvec *vec, size_t num,
                    size_t size, int flags);*/

    printk(KERN_ALERT "Event Flags: %d\n", events);
	/* Run throught he events to process */
	if (events & POLLIN) {
        /* If we already have data on the connection, make sure to append */
		if (conn->request.hdr) {
            printk(KERN_ALERT "Appending to Buffer\n");
			hdrlen = conn->request.hdrlen;
			vec.iov_base = &conn->request.hdr->buffer[hdrlen];
			vec.iov_len = MAX_INPUT_SIZE - hdrlen;
		} else {
			/* Else setup the new buffer */
			/* conn->request.hdr = http_header_alloc();
			 * vec.iov_base = &conn->request.hdr->buffer;*/
			vec.iov_base = &tmp_buffer;
			vec.iov_len = MAX_INPUT_SIZE ; 
		}

		/* Get the message, don't wait (we will come back if we need too!) */
		len = kernel_recvmsg(conn->csock, &msg, &vec, 1, MAX_INPUT_SIZE, MSG_DONTWAIT);
		
		/* Append the message */
		if (len > 0) {
			/*conn->request.hdr->buffer[len + 1] = '\0';
			printk(KERN_ALERT "Got String: %s\n", conn->request.hdr->buffer);*/
			tmp_buffer[len + 1] = '\0';
			printk(KERN_ALERT "Got string: %s\n", &tmp_buffer[0]);
		}
		if (len == EAGAIN) 
			printk(KERN_ALERT "EAGAIN Eror\n");

		/* Process the message for handoff if needed */
	}

	/* Remove the socket from the list */
	if (events & POLLHUP || events & POLLERR) {
		printk(KERN_ALERT "Removing Connection: %u.%u.%u.%u\n", NIPQUAD(sk->daddr));
	}

	/* We are done processing them, free the item we where processing */
	event_process_free(ep);
}
