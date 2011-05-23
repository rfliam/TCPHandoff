#ifndef _TCPHA_FE_HTTP_H_
#define _TCPHA_FE_HTTP_H_

/* 4kb */
#define MAX_HEADER_SIZE 4096
/* Leave room for me to add a null */
#define MAX_INPUT_SIZE 4095
struct http_request {
	struct http_header *hdr;
	int hdrlen;
	char* body;
	int bodylen;
};

/* Note if performance is an issue we should consider decreasing to 2k 
 * (still a very large header) and making this "static" in http_request.
 * Also @2k apiece a server with 128GB of ram could put 66million of these
 * in memory so v0v */
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
