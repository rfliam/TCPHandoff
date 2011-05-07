/*
 * tcpha_fe_server.h
 * This file is responsible for the front end server
 * which accepts and manages the connections. It handles the primary socket
 * an the worker threads on other sockets.
 *  Created on: Mar 24, 2011
 *      Author: rfliam200
 */

#ifndef TCPHA_FE_SERVER_H_
#define TCPHA_FE_SERVER_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <net/inet_connection_sock.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <linux/tcp.h>
#include <linux/netfilter.h>
#include <linux/list.h>

#define TCPHA_MAX_CONNECTIONS 65536

/* This is the structure representing open server connections */
/* This structure represents the configuration of the server. */
struct tcpha_fe_server_config {
    __u32 raddr;      			/* eth address */
    __u32 vaddr;				/* virtual address,which is shared in cluster */
    __u16 port;				/* listening port,typically 80 */

    int start_servers;			/* number of work threads started */
    int max_spare_servers;	/* max number of idle threads */
    int min_spare_servers;		/* min number of idle threads */

    int max_clients;			/* max clients permitted */
};

struct herder_list;

/* This is the structure representing a server */
struct tcpha_fe_server {
    /* server configuration */
    struct tcpha_fe_server_config conf;

    struct list_head be_list;       	/* real servers list */
    rwlock_t __be_list_lock;

    struct list_head rule_list;    	/* schedule rules list */
    rwlock_t __rule_list_lock;

    /* server control */
    int start;
    int stop;

    /* run-time variables */
    struct socket *mainsock;   	/* listen socket */
    atomic_t workercount;		/* workers counter */
    atomic_t running;				/* running flag */

	/* Herders */
	struct herder_list *herders;
};

/* The "primary" server thread responsible for keeping track
 * of the the "worker" threads else where.
 */
extern int tcpha_fe_server_daemon(void * __service);
int setup_server_socket(struct tcpha_fe_server *server);
void pull_down_server_socket(struct tcpha_fe_server *server);
#endif /* TCPHA_FE_SERVER_H_ */
