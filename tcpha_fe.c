/*
 * tcpha_frontend.c
 *
 *  Created on: Mar 22, 2011
 *      Author: rfliam200
 */

#include "tcpha_fe.h"
#include "tcpha_fe_client_connection.h"
#include "tcpha_fe_server.h"
#include "tcpha_fe_connection_processor.h"

static struct task_struct *server_task;
static struct tcpha_fe_server server;
static struct herder_list herders;
static struct workqueue_struct *processor;

/* Module initilization and setup methods */
/*---------------------------------------------------------------------------*/
static int tcpha_init(void) {
	printk(KERN_ALERT "TCPHA Startup\n");

	/* Setup our processors */
	processor_init(&processor);

	/* Startup the herder threads */
	init_connections(&herders, processor);

	/* Startup the acceptor thread */
	server.conf.port = 8080;
	server.herders = &herders;
	server_task = kthread_run(tcpha_fe_server_daemon, &server, "TCPHandoff Server");
	return 0;
}

static void tcpha_exit(void) {
	/* Kill the acceptor thread */
	if(atomic_read(&server.running) && kthread_stop(server_task))
		printk(KERN_ALERT "Server Failed to Unload?");

	/* Kill the herder threads */
	destroy_connections(&herders);

	processor_destroy(processor);

	printk(KERN_ALERT "TCPHA Done\n");
}

/* Module macros */
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("RICHARD FLIAM");
module_init(tcpha_init);
module_exit(tcpha_exit);
