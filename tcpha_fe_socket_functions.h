#ifndef TCPHA_FE_CLIENT_CONNECTION_H_
#define TCPHA_FE_CLIENT_CONNECTION_H_

#include <linux/version.h>

/* Functions here where added to later versions of the kernel
 * and are only used/defined if need be */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,2)
#define TCPHA_SOCKET_FUNCTIONS
extern int kernel_bind(struct socket *sock, struct sockaddr *addr,
		       int addrlen);
extern int kernel_listen(struct socket *sock, int backlog);
extern int kernel_accept(struct socket *sock, struct socket **newsock,
			 int flags);
extern int kernel_connect(struct socket *sock, struct sockaddr *addr,
			  int addrlen, int flags);
extern int kernel_getsockname(struct socket *sock, struct sockaddr *addr,
			      int *addrlen);
extern int kernel_getpeername(struct socket *sock, struct sockaddr *addr,
			      int *addrlen);
extern int kernel_getsockopt(struct socket *sock, int level, int optname,
			     char *optval, int *optlen);
extern int kernel_setsockopt(struct socket *sock, int level, int optname,
			     char *optval, unsigned int optlen);
extern int kernel_sendpage(struct socket *sock, struct page *page, int offset,
			   size_t size, int flags);
extern int kernel_sock_ioctl(struct socket *sock, int cmd, unsigned long arg);
extern int kernel_sock_shutdown(struct socket *sock,
				enum sock_shutdown_cmd how);
#endif
#endif
