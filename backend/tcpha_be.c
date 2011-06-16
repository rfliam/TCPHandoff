#include "tcpha_be.h"
#include "tcpha_be_fe_connection.h"
#include "tcpha_be_debug.h"

/* Module initilization and setup methods */
/*---------------------------------------------------------------------------*/

struct tcpha_be_server server;
struct task_struct *server_task;

static int tcpha_be_init(void) {
    /* Ip address we use */
    u8 c1, c2, c3, c4;
    c1 = 10;
    c2 = 252;
    c3 = 31;
    c4 = 32;

    server.lport = 8080;
    /* Compile time constant so v0v on efficency */
    server.laddr = c4 + (c3 * 256) + (c2 * 256 * 256) + (c1 * 256 * 256 * 256); 
    
    dtbe_printk(KERN_ALERT "Finding user space socket\n");
    /* Lookup the user space listening socket */
    server.user_sock = inet_lookup_listener(&tcp_hashinfo, server.laddr, server.lport, 0);    

    dtbe_printk(KERN_ALERT "Spooling up server\n");
    /* Startup the acceptor thread */
	server_task = kthread_run(tcpha_be_server_daemon, &server, "TCPHandoff BE Server");
	return 0;
}

static void tcpha_be_exit(void) {
	int err;
    dtbe_printk(KERN_ALERT "Stopping Server\n");
	err = kthread_stop(server_task);
    dtbe_printk(KERN_ALERT "Killing Connections\n");
	stop_fe_connections(&server);
}

/* Module macros */
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("RICHARD FLIAM");
module_init(tcpha_be_init);
module_exit(tcpha_be_exit);
