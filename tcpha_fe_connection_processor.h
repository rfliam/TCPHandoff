#ifndef _TCPHA_FE_CONNECTION_PROCESSOR_H_
#define _TCPHA_FE_CONNECTION_PROCESSOR_H_

#include <linux/net.h>
#include <net/inet_sock.h>
#include <linux/workqueue.h>

#define CONNECTION_HANDOFFED 1
#define CONNECTION_HANDOFF_PERSISTENT 2
#define CONNECTION_FINISHED 4
#define CONNECTION_ALIVE 8

/** 
 * Work, and events to process, this may be refactored 
 * if it becomes a performance issue so we are not allocing 
 * and deallocing these constantly. 
 * @author rfliam200 (5/27/2011)
 */
struct event_process {
	struct tcpha_fe_conn *conn;
	unsigned int events;
	struct work_struct work;
};

/**
 * Sets up a set of processors to work on incoming 
 * data for connections. 
 * 
 * @author rfliam200 (5/27/2011)
 * 
 * @param processor A processor to create.
 * 
 * @return int Returns less than 0 if an err occured.
 */
int processor_init(struct workqueue_struct **processor);
void processor_destroy(struct workqueue_struct *processor);

/**
 * This is responsible for dealing with input as it comes through on a given connection.  
 * 
 * @author rfliam200 (5/27/2011)
 * 
 * @param data An event_process struct, used for when we queue up work from the connection 
 * herders. (Note we may want to change event_process eventually so that we are not alloc 
 * and deallocing constantly doing this). 
 */
void process_connection(void * data);

/**
 * Create an event process.
 * 
 * @author rfliam200 (5/27/2011)
 */
void event_process_alloc(struct event_process**);

/**
 * Free an event process item.
 * 
 * @author rfliam200 (5/27/2011)
 */
void event_process_free(struct event_process*);

#endif
