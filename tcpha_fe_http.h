#ifndef _TCPHA_FE_HTTP_H_
#define _TCPHA_FE_HTTP_H_

/* 8kb, with 1 spot for null terminate */
#define MAX_HEADER_SIZE 8193

struct http_request {
	struct http_header *hdr;
	int hdrlen;
	char* body;
	int bodylen;
};

struct http_header {
	char buffer[MAX_HEADER_SIZE];
	/* These all point to back in to the buffer */
	char *method;
	char *request_uri;
	char *http_version;
};


void http_init(void);
void http_destroy(void);
/* Use this method to get a header to work with */
struct http_header *http_header_alloc(void);
void http_header_free(struct http_header *hdr);
#endif
