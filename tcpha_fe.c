/*
 * tcpha_frontend.c
 *
 *  Created on: Mar 22, 2011
 *      Author: rfliam200
 */

#include "tcpha_fe.h"
#include "tcpha_fe_server.h"

static struct task_struct *server_task;
static struct tcpha_fe_server server;

/* Module initilization and setup methods */
/*---------------------------------------------------------------------------*/
static int tcpha_init(void) {
	printk(KERN_ALERT "TCPHA Startup\n");

	server.conf.port = 8080;
	server_task = kthread_run(tcpha_fe_server_daemon, &server, "TCPHandoff Server");
	return 0;
}

static void tcpha_exit(void) {
	/* Kill the server daemon */
	if(atomic_read(&server.running) && kthread_stop(server_task))
		printk(KERN_ALERT "Server Failed to Unload?");

	printk(KERN_ALERT "TCPHA Done\n");
}

/* Module macros */
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("RICHARD FLIAM");
module_init(tcpha_init);
module_exit(tcpha_exit);
