#include "tcpha_be.h"
#include "tcpha_be_fe_connection.h"

/* Module initilization and setup methods */
/*---------------------------------------------------------------------------*/

struct tcpha_be_server server;
struct task_struct *server_task;

static int tcpha_be_init(void) {
    server.lport = 8080;
    
    /* Lookup the user space listening socket */
        

    /* Startup the acceptor thread */
	server_task = kthread_run(tcpha_be_server_daemon, &server, "TCPHandoff BE Server");
	return 0;
}

static void tcpha_be_exit(void) {
	int err;
	err = kthread_stop(server_task);
	stop_fe_connections(&server);
}

/* Module macros */
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("RICHARD FLIAM");
module_init(tcpha_be_init);
module_exit(tcpha_be_exit);
