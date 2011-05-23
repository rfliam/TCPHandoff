#include "tcpha_fe_http.h"
#include <linux/slab.h>

struct kmem_cache *header_cache_ptr;

void http_init(void)
{
	header_cache_ptr = kmem_cache_create("tcpha http hdr cache", 
						sizeof(struct http_header),
			  0, SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL, NULL);

	if (!header_cache_ptr)
		printk(KERN_ALERT "Error getting http_header memcache\n");
}

struct http_header *http_header_alloc(void)
{
	return kmem_cache_zalloc(header_cache_ptr, GFP_KERNEL);
}
void http_header_free(struct http_header *hdr)
{
	return kmem_cache_free(header_cache_ptr, hdr);
}
void http_destroy(void)
{
	kmem_cache_destroy(header_cache_ptr);
}
