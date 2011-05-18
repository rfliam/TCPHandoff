#ifndef _TCPHA_FE_CONNECTION_PROCESSOR_H_
#define _TCPHA_FE_CONNECTION_PROCESSOR_H_

#include <linux/net.h>
#include <net/inet_sock.h>
#include <linux/workqueue.h>

#define CONNECTION_HANDOFFED 1
#define CONNECTION_HANDOFF_PERSISTENT 2
#define CONNECTION_FINISHED 4
#define CONNECTION_ALIVE 8

struct event_process {
	struct tcpha_fe_conn *conn;
	unsigned int events;
	struct work_struct work;
};

int processor_init(struct workqueue_struct **processor);
void processor_destroy(struct workqueue_struct *processor);

void process_connection(void * data);

void event_process_alloc(struct event_process**);
void event_process_free(struct event_process*);

#endif
