#define TCPHA_BE_DEBUG
#ifdef TCPHA_BE_DEBUG
#define dtbe_printk(x...) do { printk(x); } while (0)
#else
#define dtbe_printk(x...)  do {} while (0)
#endif
