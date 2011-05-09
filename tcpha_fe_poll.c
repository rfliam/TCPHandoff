#include "tcpha_fe_poll.h"

/* Developer Notes:
 *  The structure lock in tcp_eventpoll "lock" is used to protect against concurrent
 *  modification of the epoll. The other two locks are to protect against the fact that
 *  we modify the eventpoll (wake, etc.) in an interrupt context. In particular the
 *  ready list lock list_lock and the epoll item lock "lock". We use the irq variants
 *  of the spinlocks as they disable interrupts during there operation. Hold them for
 *  as short a time as possible!.
 */
/* Memory cache for epoll items */
struct kmem_cache *tcp_ep_item_cachep = NULL;
atomic_t item_cache_use = ATOMIC_INIT(0);

/* Every socket we are epolling gets one of these linked in to the hash */
struct tcp_ep_item {
	/* Structure Lock, should be got with an IRQ lock
	 * when modifying event_flags and rd_list_link. */
	rwlock_t lock;

	/* The sock this represents */
	struct socket *sock;

	/* Tie us into the RB-Tree hash */
	struct rb_node hash_node;
	
	/* Used to tie this into the ready list */
	struct list_head rd_list_link;

	/* Struct describing the events we are interested in */
	unsigned int event_flags;
	
	/* The eventpoll this item is tied too */
	struct tcp_eventpoll *eventpoll;

	/* Our wait queue */
	wait_queue_t wait;

	/* Wait queue head we are linked to */
	wait_queue_head_t *whead;
};

/* Private Method Prototypes */
/*---------------------------------------------------------------------------*/
/* Constructor/destructors for our structs */
static int tcp_ep_item_alloc(struct tcp_ep_item **item);
static void tcp_ep_item_destroy(struct tcp_ep_item *item);
static inline void tcp_ep_item_free(struct tcp_ep_item *item);

static inline int tcp_epoll_alloc(struct tcp_eventpoll **eventpoll);
static inline void tcp_epoll_free(struct tcp_eventpoll *eventpoll);

/* Utility methods */
/* Called by the socket wakequeue when actvitiy occurs on it, determines if 
 * we should wake any sleepers and add item to readylist */
static int tcp_epoll_wakeup(wait_queue_t *curr, unsigned mode, int sync, void *key);
static inline unsigned int tcp_epoll_check_events(struct tcp_ep_item *item);
static inline struct tcp_ep_item *tcp_ep_item_from_wait(wait_queue_t *p);
static inline void add_item_to_readylist(struct tcp_ep_item *item);
static inline void remove_item_from_readylist(struct tcp_ep_item *item);

/* RBTree (hash) Usage Methods */
static int tcp_ep_hash_insert(struct tcp_ep_item *item);
static struct tcp_ep_item *tcp_ep_hash_find(struct tcp_eventpoll *eventpoll, struct socket *sock);

static inline void tcp_ep_rb_initnode(struct rb_node *n);
static inline void tcp_ep_rb_removenode(struct rb_node *n, struct rb_root *r);
static inline int tcp_cmp_sock(struct socket *leftsock, struct socket *rightsock);

/* Constructor/Destructor methods */
/*---------------------------------------------------------------------------*/
int tcp_epoll_init(struct tcp_eventpoll **eventpoll)
{
	int err;
	struct tcp_eventpoll *ep;

	err = tcp_epoll_alloc(eventpoll);
	if (err)
		return err;
	
	ep = *eventpoll;
	init_waitqueue_head(&ep->poll_wait);
	INIT_LIST_HEAD(&ep->ready_list);
	rwlock_init(&ep->lock);
	rwlock_init(&ep->list_lock);
	ep->hash_root = RB_ROOT;

	/* Guard against multiple initilization, make it for the first user */
	if (atomic_inc_return(&item_cache_use) == 1) {
		tcp_ep_item_cachep = kmem_cache_create("tcp_ep_itemcache", 
										   sizeof(struct tcp_ep_item), 
																	0,
										SLAB_HWCACHE_ALIGN|SLAB_PANIC,
														   NULL, NULL);
	}
	return 0;
}

static inline int tcp_epoll_alloc(struct tcp_eventpoll **eventpoll)
{
	struct tcp_eventpoll *ep = kzalloc(sizeof(struct tcp_eventpoll), GFP_KERNEL);

	if (!ep)
		return -ENOMEM;

	*eventpoll = ep;
	return 0;
}

void tcp_epoll_destroy(struct tcp_eventpoll *ep)
{
	struct rb_node *node;
	/* Cleanup epoll items in the hash */
	for (node = rb_first(&ep->hash_root); node; node = rb_next(node))
		tcp_ep_item_destroy(rb_entry(node, struct tcp_ep_item, hash_node)); ;

	tcp_epoll_free(ep);
	/* Destroy for the last user */
	if (atomic_dec_and_test(&item_cache_use)) {
			kmem_cache_destroy(tcp_ep_item_cachep);
	}
}

static inline void tcp_epoll_free(struct tcp_eventpoll *eventpoll)
{
	if (eventpoll)
		kfree(eventpoll);
}

static int tcp_ep_item_alloc(struct tcp_ep_item **item)
{
	struct tcp_ep_item *epi;

	if (!tcp_ep_item_cachep)
		return -1;

	epi = kmem_cache_alloc(tcp_ep_item_cachep, SLAB_KERNEL);

	if(!epi)
		return -ENOMEM;

	rwlock_init(&epi->lock);
	tcp_ep_rb_initnode(&epi->hash_node);
	INIT_LIST_HEAD(&epi->rd_list_link);
	epi->event_flags = 0;
	*item = epi;
	return 0;
}

static void tcp_ep_item_destroy(struct tcp_ep_item *item)
{
	struct tcp_eventpoll *ep = item->eventpoll;
	int flags;
	/* Now remove ourseleves from any poll stuff */
	/* We let go of the lock quickly since no one else should now cause
	 * concurrent modification to the item */
	write_lock_irqsave(&item->lock, flags);
	remove_wait_queue(item->whead, &item->wait);
	write_unlock_irqrestore(&item->lock, flags);
	
	/* Delete the item from the hash and readylist */
	write_lock(&ep->lock);
	tcp_ep_rb_removenode(&item->hash_node, &ep->hash_root);
	write_unlock(&ep->lock);

	/* Locks for us */
	remove_item_from_readylist(item); 
		
	/* Delete the item */
	tcp_ep_item_free(item);
}

static inline void tcp_ep_item_free(struct tcp_ep_item *item)
{
	if (item)
		kmem_cache_free(tcp_ep_item_cachep, item);
}

/* Modification and Usage Methods (You can get to these from outside) */
/*---------------------------------------------------------------------------*/
int tcp_epoll_insert(struct tcp_eventpoll *eventpoll, struct socket *sock, unsigned int flags)
{
	int err;
	unsigned int mask;
	struct tcp_ep_item *item = NULL;

	/* Allocate our item */
	err = tcp_ep_item_alloc(&item);
	if(unlikely(err))
		return err;
	
	/* Setup our item, no need to lock because no one else could POSSIBLY
	 * have it yet. */
	item->sock = sock;
	item->event_flags = flags | POLLERR | POLLHUP;
	item->eventpoll = eventpoll;

	/* And set it up to add itself to the readylist when appropriate */
	init_waitqueue_func_entry(&item->wait, tcp_epoll_wakeup);
	item->whead = sock->sk->sk_sleep;

	/* Add it to the hash*/
	write_lock(&eventpoll->lock);
	err = tcp_ep_hash_insert(item);
	write_unlock(&eventpoll->lock);
	if (err)
		goto insert_fail;

	/* If its already ready stitch it into the ready list */
	mask = tcp_epoll_check_events(item);
	if (mask)
		add_item_to_readylist(item); /* Locks for us */

	/* Done last so no contention problems */
	add_wait_queue(item->whead, &item->wait);

	return 0;

	/* May occur if the ep goes away while we are trying to insert
	 * an item. */
insert_fail:
	tcp_ep_item_free(item);
	return err;
}

/* Remove the item from all relevant structs etc */
void tcp_epoll_remove(struct tcp_eventpoll *ep, struct socket *sock)
{
	struct tcp_ep_item *item;
	
	/* First find the item for the struct in our RB Tree */
	read_lock(&ep->lock);
	item = tcp_ep_hash_find(ep, sock);
	read_unlock(&ep->lock);
	if (!item)
		return;

	tcp_ep_item_destroy(item);	
}

int tcp_epoll_setflags(struct tcp_eventpoll *eventpoll, struct socket *sock, unsigned int flags)
{
	/* Find the item, and change its flags */
	return -1;
}

int tcp_epoll_wait(struct tcp_eventpoll *eventpoll, struct socket *sockets[], int maxevents, int timeout)
{
	return -1;
}

/* Private Other Methods */
/*---------------------------------------------------------------------------*/

/* This method REQUIRES to hold the proper locks on item */
static inline unsigned int tcp_epoll_check_events(struct tcp_ep_item *item)
{
	/* Nice little trick, by calling poll without a poll table..
	 * poll returns immediately on tcp sockets (see poll_wait)
	 * and the file is never used :) */
	return item->event_flags & item->sock->ops->poll(NULL, item->sock, NULL);
}

static int tcp_epoll_wakeup(wait_queue_t *curr, unsigned mode, int sync, void *key)
{
	unsigned int mask = 0;
	struct tcp_ep_item *item;

	item = tcp_ep_item_from_wait(curr);
	mask = tcp_epoll_check_events(item);

	if (mask) {
		printk(KERN_ALERT "Should Wake");
	}

	return 1;
}

static inline struct tcp_ep_item *tcp_ep_item_from_wait(wait_queue_t *p)
{
	return container_of(p, struct tcp_ep_item, wait);
}

/* Item must be properly locked if necessary, rddlist will be lock by method */
static inline void add_item_to_readylist(struct tcp_ep_item *item)
{
	int flags;
	struct tcp_eventpoll *ep = item->eventpoll;
	/* in demand, hold for as short a time as  possible */
	write_lock_irqsave(&ep->list_lock, flags);
	list_add_tail(&item->rd_list_link, &ep->ready_list);
	write_unlock_irqrestore(&ep->list_lock, flags);
}

static inline void remove_item_from_readylist(struct tcp_ep_item *item)
{
	int flags;
	struct tcp_eventpoll *ep = item->eventpoll;
	/* in demand, hold for as short a time as  possible */
	write_lock_irqsave(&ep->list_lock, flags);
	list_del(&item->rd_list_link);
	write_unlock_irqrestore(&ep->list_lock, flags);
}
/* RBTree Methods */
/*---------------------------------------------------------------------------*/
/* All of the blow methods work on the assumption that the caller has done the correct locking */
static int tcp_ep_hash_insert(struct tcp_ep_item *item)
{
	int cmp;
	struct tcp_eventpoll *ep = item->eventpoll;
	struct rb_node **p = &ep->hash_root.rb_node;
	struct rb_node *parent = NULL;
	struct tcp_ep_item *epic;

	while (*p) {
		parent = *p;
		epic = rb_entry(parent, struct tcp_ep_item, hash_node);
		cmp = tcp_cmp_sock(epic->sock, item->sock);
		if (cmp == 0)
			return -1;
		p = cmp > 0 ? &parent->rb_right : &parent->rb_left;
	}

	rb_link_node(&item->hash_node, parent, p);
	rb_insert_color(&item->hash_node, &ep->hash_root);

	return 0;
}

static inline void tcp_ep_rb_initnode(struct rb_node *n)
{
	rb_set_parent(n, n);
}

static inline void tcp_ep_rb_removenode(struct rb_node *n, struct rb_root *r)
{
	rb_erase(n, r);
	rb_set_parent(n, n);
}

static struct tcp_ep_item *tcp_ep_hash_find(struct tcp_eventpoll *eventpoll, struct socket *sock)
{
	struct rb_node *rbn;
	struct tcp_ep_item *item = NULL;
	int cmp;
	int found = 0;

	/* Start at root, traverse tree */
	rbn = eventpoll->hash_root.rb_node;
	while (rbn) {
		item = rb_entry(rbn, struct tcp_ep_item, hash_node);
		cmp = tcp_cmp_sock(sock, item->sock);
		if (cmp == 0) {
			found = 1;
			break;
		}
		rbn = cmp < 0 ? rbn->rb_left : rbn->rb_right;
	}

	/* If not found return null */
	return found ? item : NULL;
}

static inline int tcp_cmp_sock(struct socket *leftsock, struct socket *rightsock)
{
	struct inet_sock *leftsk = inet_sk(leftsock->sk);
	struct inet_sock *rightsk = inet_sk(rightsock->sk);

	/* This compares both the daddr and dport, if both match
	 * the sockets are the same */
	__u32 diff = leftsk->daddr - rightsk->daddr;

	if(!diff)
	{
		diff = leftsk->dport - rightsk->dport;
	}
	
	return diff;
}
