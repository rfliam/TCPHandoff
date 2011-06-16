#include "tcpha_be_handoff_connection.h"
#include "tcpha_be_fe_connection.h"
/* Prototypes */

/* The command handlers */
/*---------------------------------------------------------------------------*/
static bool new_conn(struct tcpha_be_fe_connection *conn);
static bool modify_conn(struct tcpha_be_fe_connection *conn);
static bool rx_on_conn(struct tcpha_be_fe_connection *conn);
static bool remove_conn(struct tcpha_be_fe_connection *conn);

/* Private life cycle methods */
/*---------------------------------------------------------------------------*/
static inline void handoff_conn_alloc(struct tcpha_be_handoff_connection **conn);
static inline void handoff_conn_init(struct tcpha_be_handoff_connection **conn);
static inline void handoff_conn_free(struct tcpha_be_handoff_connection *conn);

/* Utility Methods */
/*---------------------------------------------------------------------------*/
static void create_sk(struct sock **newsk, struct sock *source_sk);
static inline void setup_inet_sk(struct inet_sock *dest, struct inet_sock *source);
static inline void setup_inet_connection_sk(struct inet_connection_sock *dest, 
                                     struct inet_connection_sock *source);
static inline void setup_tcp_sk(struct tcp_sock *dest, struct tcp_sock *source);

/* Command Handlers Table Etc. */
/*---------------------------------------------------------------------------*/
#define NEW 0
#define MODIFY 1
#define RX 2
#define REMOVE 3
static bool (*cmd_table[])(struct tcpha_be_fe_connection *conn) = {&new_conn, &modify_conn, &rx_on_conn,
&remove_conn};

/* Implementations */
/*---------------------------------------------------------------------------*/
bool process_data_for_connection(struct tcpha_be_fe_connection *conn)
{
	if (conn->hdr.cmd < sizeof(cmd_table))
		return cmd_table[conn->hdr.cmd](conn);
	return false;
}

static bool new_conn(struct tcpha_be_fe_connection *conn)
{
	struct tcpha_be_handoff_connection *hac;
    struct sock *new_sock, *buffer_sk;

    /* For now just make and stitch into list, should be radix tree */
	handoff_conn_init(&hac);
	list_add(&hac->list, &conn->handoff_conn_list);

	if (conn->ipv4hdr.len < sizeof(struct tcp_sock))
		return false;

	/* Create our socket */
    /* We use sock create lite and do a manual setup here... */
    buffer_sk = (struct sock*)conn->buffer[11];
    create_sk(&new_sock, buffer_sk);

    /* Test to see if the newely created sock(et) is usable... */
   	/* Grab the user space listening socket and add ourselves to it */
    /* No fanciness necessary here, just adding to the listening socket
       is sufficient */
    
    /* Rx the data on the socket */

	return false;
}

static bool modify_conn(struct tcpha_be_fe_connection *conn)
{
	return false;
}
static bool rx_on_conn(struct tcpha_be_fe_connection *conn)
{
	return false;
}
static bool remove_conn(struct tcpha_be_fe_connection *conn)
{
	return false;
}

/* Utility Methods */
/*---------------------------------------------------------------------------*/
static inline void setup_inet_sk(struct inet_sock *dest, struct inet_sock *source)
{
	dest->saddr = source->saddr;
    dest->sport = source->sport;
    dest->daddr = source->daddr;
    dest->dport = source->dport;	
}

static inline void setup_inet_connection_sk(struct inet_connection_sock *dest, 
                                     struct inet_connection_sock *source)
{
    /* Mostly ripped from inet_csk_clone */
    dest->icsk_bind_hash = NULL;
    dest->icsk_retransmits = source->icsk_retransmits;
    dest->icsk_backoff = source->icsk_backoff;
    dest->icsk_probes_out = source->icsk_probes_out;

    /* This is not a listening socket, de-initialize to stop illegal accesses. */
    memset(&dest->icsk_accept_queue, 0, sizeof(dest->icsk_accept_queue));
}


static inline void setup_tcp_sk(struct tcp_sock *dest, struct tcp_sock *source)
{
    /* A straight up copy of the TCP options (since the structure is "inherited"
       we have to trim out the first member) */
    int bytes_to_copy = sizeof(struct tcp_sock) - sizeof(struct inet_connection_sock);
    char *cp_dest = ((char*)dest) + sizeof(struct inet_connection_sock);
    char *cp_src =  ((char*)source) + sizeof(struct inet_connection_sock);
    memcpy(cp_dest, cp_src, bytes_to_copy);
}

static void create_sk(struct sock **newsk, struct sock *source)
{
    struct sock *nsk = sk_clone(source, GFP_KERNEL);

    if (!nsk) {
        *newsk = NULL;
        return;
    }

    setup_inet_sk(inet_sk(nsk), inet_sk(nsk));
    setup_inet_connection_sk(inet_csk(nsk), inet_csk(nsk)); 
    setup_tcp_sk(tcp_sk(nsk), tcp_sk(nsk));

    *newsk = nsk;
}


/* Private life cycle methods */
/*---------------------------------------------------------------------------*/
inline void handoff_conn_alloc(struct tcpha_be_handoff_connection **conn)
{
	/* TODO: Should be coming from a kmemcache */
	*conn = kzalloc(sizeof(struct tcpha_be_handoff_connection), GFP_KERNEL);
}
inline void handoff_conn_init(struct tcpha_be_handoff_connection **conn)
{
	handoff_conn_alloc(conn);
	INIT_LIST_HEAD(&(*conn)->list);
}

inline void handoff_conn_free(struct tcpha_be_handoff_connection *conn)
{
	kfree(conn);
}


