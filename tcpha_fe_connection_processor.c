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
	char buffer[1024];
	struct tcpha_fe_conn *conn = data;
	struct inet_sock *sk = inet_sk(conn->csock->sk);

	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	vec.iov_base = &buffer;
	vec.iov_len = 1024;

	printk(KERN_ALERT "Work done on connection %u.%u.%u.%u\n", NIPQUAD(sk->daddr));
	/*int kernel_recvmsg(struct socket *sock, struct msghdr *msg, 
                    struct kvec *vec, size_t num,
                    size_t size, int flags);*/
	
	len = kernel_recvmsg(conn->csock, &msg, &vec, 1, 1024, MSG_DONTWAIT);
	if (len > 0 && len < 1023) {
		buffer[len + 1] = '\0';
		printk(KERN_ALERT "Got String: %s\n", buffer);
	}
	if (len == EAGAIN) 
		printk(KERN_ALERT "EAGAIN Eror\n");
}
