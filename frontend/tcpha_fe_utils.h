#ifndef _TCPHA_FE_UTILS_H_
#define _TCPHA_FE_UTILS_H_
#include <linux/poll.h>

#define POLLMASK(mask) \
        POLLIN & mask, \
        POLLPRI & mask, \
        POLLOUT & mask, \
        POLLERR & mask, \
        POLLHUP & mask, \
        POLLNVAL & mask, \
        POLLRDNORM & mask, \
        POLLRDBAND & mask, \
        POLLWRNORM & mask, \
        POLLWRBAND & mask, \
        POLLMSG & mask, \
        POLLREMOVE & mask, \
        POLLRDHUP & mask

#define POLLMASKFMT "\
        POLLIN:     0x%x\n\
        POLLPRI:    0x%x\n\
        POLLOUT:    0x%x\n\
        POLLERR:    0x%x\n\
        POLLHUP:    0x%x\n\
        POLLNVAL:   0x%x\n\
        POLLRDNORM: 0x%x\n\
        POLLRDBAND: 0x%x\n\
        POLLWRNORM: 0x%x\n\
        POLLWRBAND: 0x%x\n\
        POLLMSG:    0x%x\n\
        POLLREMOVE: 0x%x\n\
        POLLRDHUP:  0x%x\n"
                    
#endif
