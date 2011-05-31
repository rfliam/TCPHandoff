#include "tcpha_fe_http.h"
#include "tcpha_fe_client_connection.h"
#include <linux/slab.h>

struct kmem_cache *header_cache_ptr;

void http_init(void)
{
	/*
	header_cache_ptr = kmem_cache_create("tcpha http hdr cache", 
						sizeof(struct http_header),
			  0, SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL, NULL);

	if (!header_cache_ptr)
		printk(KERN_ALERT "Error getting http_header memcache\n");*/
}

struct http_header *http_header_alloc(void)
{
	/*return kmem_cache_zalloc(header_cache_ptr, GFP_KERNEL);*/
	return kzalloc(sizeof(struct http_header), GFP_KERNEL);
}
void http_header_free(struct http_header *hdr)
{
	kfree(hdr);
	/*return kmem_cache_free(header_cache_ptr, hdr);*/
}
void http_destroy(void)
{
	/*kmem_cache_destroy(header_cache_ptr);*/
}

/* NOTE: For the purposes of routing, we ignore the remaining
   requests in a pipelined HTTP request. This may be subject to change
   in the future. */
int http_process_connection(struct tcpha_fe_conn *conn)
{
    int i = 0;
    int state = 0;
    int hdrlen = conn->request.hdrlen;
    int hash = 0;
    /* Ignore the method (We don't care), set path only
    at least for now */
    for (; i < hdrlen && conn->request.hdr->buffer[i] != ' '; i++) {
    }

    if (!(i + 1 < hdrlen)) {
        return HDR_READ_ERROR;
    } else {
        conn->request.hdr->request_uri = &conn->request.hdr->buffer[i + 1];
    }

    /* TODO: Option to ignore query... (eg. stop at ?) */
    for (; i < hdrlen && conn->request.hdr->buffer[i] != ' '; i++) {
        hash = 31 * hash + (int)conn->request.hdr->buffer[i];
    }

    if (!(i < hdrlen)) {
        return HDR_READ_ERROR;
    } else {
        conn->request.hdr->uri_len = i;
    }

    /* Now look for \r\n\r\n  indicating we have a full http request */
    for (; i < hdrlen; i++) {
        if (conn->request.hdr->buffer[i] == '\r') {
            state++;
        }
        if (conn->request.hdr->buffer[i] == '\n' && (state == 1 || state == 3)) {
            state++;
        }
        if (state == 4) {
            return hash;
        }
    }

    return HDR_READ_ERROR;
}
