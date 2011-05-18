#include "tcpha_fe_connection_processor.h"
#include "tcpha_fe_client_connection.h"
#include "tcpha_fe_socket_functions.h"

/* Constructore/destructor methods */
/*---------------------------------------------------------------------------*/

int processor_init(struct workqueue_struct **processor)
{
	*processor = create_workqueue("TCPHA_Connection_Processor");
	return 0;
}

void processor_destroy(struct workqueue_struct *processor)
{
	flush_workqueue(processor);
	destroy_workqueue(processor);
}


/* External (public) methods */
/*---------------------------------------------------------------------------*/

void process_connection(void * data)
{
	struct kvec vec;
	struct msghdr msg;
	int len;

	msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
	vec.iov_base = ;
	vec.iov_len = ;
	struct tcpha_fe_conn *conn = data;
	struct inet_sock *sk = inet_sk(conn->csock->sk);
	printk(KERN_ALERT "Work done on connection %u.%u.%u.%u", NIPQUAD(sk->daddr));
	/*int kernel_recvmsg(struct socket *sock, struct msghdr *msg, 
                    struct kvec *vec, size_t num,
                    size_t size, int flags);*/
	kernel_recvmsg(conn->csock, msg, vec, 1, msg.msg_flags);
}
