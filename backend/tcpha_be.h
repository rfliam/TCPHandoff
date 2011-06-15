#ifndef _TCPHA_BE_H_
#define _TCPHA_BE_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <asm/atomic.h>
#include <asm/spinlock.h>

/**
 * This struct is responsible for setting up connections 
 * with the front end. 
 *  
 * @author rfliam200 (6/7/2011)
 */
struct tcpha_be_server {
    unsigned int lport;
    unsigned int laddress;
    struct socket *listener;
    struct list_head fe_connections_list;
    spinlock_t fe_connections_lock;
    unsigned int num_fe_connections;
    atomic_t running;
};

#endif
