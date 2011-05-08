#ifndef TCPHA_FE_POLL_H_
#define TCPHA_FE_POLL_H_

/*
 * This is a port of the epoll mechanism to work in kernel with socket structs.
 * This allows for vast simplification of the entire mechanism since we don't
 * need to copy to user space, or need a virtual filesystem. We also side step
 * some complexity because we do not expect multiple epollers to be working on the
 * same socket, or for epolls to occur from multiple threads.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/rbtree.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/net.h>
#include <net/inet_sock.h>
#include <asm/atomic.h>

/* The primary structure representing an epoll item */
struct tcp_eventpoll {
	/* Structure protection lock */
	/* This lock also protects the hash */
	rwlock_t lock;

	/* The head used by tcph_epoll_wait */
	wait_queue_head_t poll_wait;
	
	/* List of ready to use sockets */
	struct list_head ready_list;

	/* Conccurent modification of ready list */
	/* Provided seperately as its modified from an interrupt
	 * context */
	rwlock_t list_lock;

	/* RB-Tree used as hash table to store monitore socket structs */
	struct rb_root hash_root;
};

/* Epoll setup and destroy */
extern int tcp_epoll_init(struct tcp_eventpoll **eventpoll);
extern void tcp_epoll_destroy(struct tcp_eventpoll *eventpoll);

/* Methods to add/remove/modify sockets */
extern int tcp_epoll_insert(struct tcp_eventpoll *eventpoll, struct socket *sock, unsigned int flags);
extern void tcp_epoll_remove(struct tcp_eventpoll *eventpoll, struct socket *sock);
extern int tcp_epoll_setflags(struct tcp_eventpoll *eventpoll, struct socket *sock, unsigned int flags);

/* Polling the epoll */
extern int tcp_epoll_wait(struct tcp_eventpoll *eventpoll, struct socket *sockets[], int maxevents, int timeout);

#endif
