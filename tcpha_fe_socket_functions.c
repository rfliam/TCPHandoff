#include "tcpha_fe_socket_functions.h"
#ifdef TCPHA_SOCKET_FUNCTIONS
int kernel_bind(struct socket *sock, struct sockaddr *addr, int addrlen)
{
	return sock->ops->bind(sock, addr, addrlen);
}

int kernel_listen(struct socket *sock, int backlog)
{
	return sock->ops->listen(sock, backlog);
}

int kernel_accept(struct socket *sock, struct socket **newsock, int flags)
{
	struct sock *sk = sock->sk;
	int err;

	err = sock_create_lite(sk->sk_family, sk->sk_type, sk->sk_protocol,
			       newsock);
	if (err < 0)
		goto done;

	err = sock->ops->accept(sock, *newsock, flags);
	if (err < 0) {
		sock_release(*newsock);
		*newsock = NULL;
		goto done;
	}

	(*newsock)->ops = sock->ops;
	__module_get((*newsock)->ops->owner);

done:
	return err;
}

int kernel_connect(struct socket *sock, struct sockaddr *addr, int addrlen,
		   int flags)
{
	return sock->ops->connect(sock, addr, addrlen, flags);
}

int kernel_getsockname(struct socket *sock, struct sockaddr *addr,
			 int *addrlen)
{
	return sock->ops->getname(sock, addr, addrlen, 0);
}

int kernel_getpeername(struct socket *sock, struct sockaddr *addr,
			 int *addrlen)
{
	return sock->ops->getname(sock, addr, addrlen, 1);
}

int kernel_getsockopt(struct socket *sock, int level, int optname,
			char *optval, int *optlen)
{
	mm_segment_t oldfs = get_fs();
	char __user *uoptval;
	int __user *uoptlen;
	int err;

	uoptval = (char __user __force *) optval;
	uoptlen = (int __user __force *) optlen;

	set_fs(KERNEL_DS);
	if (level == SOL_SOCKET)
		err = sock_getsockopt(sock, level, optname, uoptval, uoptlen);
	else
		err = sock->ops->getsockopt(sock, level, optname, uoptval,
					    uoptlen);
	set_fs(oldfs);
	return err;
}

int kernel_setsockopt(struct socket *sock, int level, int optname,
			char *optval, unsigned int optlen)
{
	mm_segment_t oldfs = get_fs();
	char __user *uoptval;
	int err;

	uoptval = (char __user __force *) optval;

	set_fs(KERNEL_DS);
	if (level == SOL_SOCKET)
		err = sock_setsockopt(sock, level, optname, uoptval, optlen);
	else
		err = sock->ops->setsockopt(sock, level, optname, uoptval,
					    optlen);
	set_fs(oldfs);
	return err;
}

int kernel_sendpage(struct socket *sock, struct page *page, int offset,
		    size_t size, int flags)
{
	sock_update_classid(sock->sk);

	if (sock->ops->sendpage)
		return sock->ops->sendpage(sock, page, offset, size, flags);

	return sock_no_sendpage(sock, page, offset, size, flags);
}

int kernel_sock_ioctl(struct socket *sock, int cmd, unsigned long arg)
{
	mm_segment_t oldfs = get_fs();
	int err;

	set_fs(KERNEL_DS);
	err = sock->ops->ioctl(sock, cmd, arg);
	set_fs(oldfs);

	return err;
}

int kernel_sock_shutdown(struct socket *sock, enum sock_shutdown_cmd how)
{
	return sock->ops->shutdown(sock, how);
}
#endif
