#ifndef _TCPHA_FE_HTTP_H_
#define _TCPHA_FE_HTTP_H_

/* 4kb */
#define MAX_HEADER_SIZE 4096
/* Leave room for me to add a null */
#define MAX_INPUT_SIZE 4095
#define HDR_READ_ERROR -13;
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
	char *request_uri;
    int uri_len;
};


void http_init(void);
void http_destroy(void);
/* Use this method to get a header to work with */
struct http_header *http_header_alloc(void);
void http_header_free(struct http_header *hdr);


struct tcpha_fe_conn;

/**
 * Work on a header.
 * 
 * @author rfliam200 (6/1/2011)
 * 
 * @param conn The connection (with already exsisting header to 
 *             work on).
 * @param hash A integer to fill with the computed hash value.
 * 
 * @return int 0 if a complete and correct header was read, 
 *         HDR_READ_ERROR otherwise.
 */
int http_process_connection(struct tcpha_fe_conn *conn, int *hash);
#endif
