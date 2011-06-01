#include "tcpha_fe_connection_processor.h"
#include "tcpha_fe_client_connection.h"
#include "tcpha_fe_socket_functions.h"
#include "tcpha_fe_http.h"
#include "tcpha_fe_utils.h"

struct kmem_cache *event_process_memcache_ptr;

/* Private Methods */
/*---------------------------------------------------------------------------*/
static inline void process_pollin(struct tcpha_fe_conn *conn);
static inline void process_pollrdhup(struct tcpha_fe_herder *herder, struct tcpha_fe_conn *conn);


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
/* TODO: Do we really need to be doing coping at all? Just start a state machine reading from
   the sockets input buffer? */
void process_connection(void *data)
{
    struct event_process *ep = data;
    struct tcpha_fe_conn *conn = ep->conn;
    unsigned int events = ep->events;
    struct inet_sock *sk = inet_sk(conn->csock->sk);

    printk(KERN_ALERT "Working on connection %u.%u.%u.%u\n", NIPQUAD(sk->daddr));
    /* Run throught he events to process */
    if (events & POLLIN) {
        process_pollin(conn);
    }

    /* Remove the socket from the list */
    if (events & POLLRDHUP) {
        process_pollrdhup(ep->herder, conn);
    }

    /* We are done processing them, free the item we where processing */
    event_process_free(ep);
}

/* Handlers for different poll events */
/*---------------------------------------------------------------------------*/

static inline void process_pollin(struct tcpha_fe_conn *conn)
{
    struct kvec vec;
    struct msghdr msg;
    int len;
    int hdrlen;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    /* If we already have data on the connection, make sure to append */
    if (conn->request.hdr) {
        printk(KERN_ALERT "  Appending to Buffer\n");
        hdrlen = conn->request.hdrlen;
        vec.iov_base = &conn->request.hdr->buffer[hdrlen];
        vec.iov_len = MAX_INPUT_SIZE - hdrlen;
    } else {
        /* Else setup the new buffer */
        conn->request.hdr = http_header_alloc();
        conn->request.hdrlen = 0;
        vec.iov_base = &conn->request.hdr->buffer;
    }

    /* Get the message, don't wait (we will come back if we need too!) */
    len = kernel_recvmsg(conn->csock, &msg, &vec, 1, MAX_INPUT_SIZE, MSG_DONTWAIT);
    hdrlen = conn->request.hdrlen + len;
    /* Append the message */
    if (len > 0 && hdrlen < MAX_INPUT_SIZE) {
        conn->request.hdr->buffer[hdrlen + 1] = '\0';
        conn->request.hdrlen = hdrlen;
        printk(KERN_ALERT "   Buffer Now: %s\n\n", conn->request.hdr->buffer);
    }

    /* Process the message for handoff if needed */
}

static inline void process_pollrdhup(struct tcpha_fe_herder *herder, struct tcpha_fe_conn *conn)
{
    struct inet_sock *sk = inet_sk(conn->csock->sk);
    if (atomic_dec_and_test(&conn->alive)) {
        printk(KERN_ALERT "   Removing Connection: %u.%u.%u.%u\n", NIPQUAD(sk->daddr));
        tcpha_fe_conn_destroy(herder, conn);
    }
}
