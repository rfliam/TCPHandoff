
/**
 * This is the starting point for the frontend of TCPHA. In particular it
 * defines the methods used for starting/exiting the kernel module and hooking into
 * the TCP stack with the netfilter API. The
 */
#ifndef TCPHA_FRONTEND_H
#define TCPHA_FRONTEND_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>

/**
 * These methods are responsible for the
 * hooks for the kernel module load in.
 */
static int tcpha_init(void);
static void tcpha_exit(void);

#endif
