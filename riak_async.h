#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include "riak_types.h"

typedef void (*riak_event_fn)(void *ctx);
typedef void (*riak_result_fn)(void *ctx);

typedef struct _riak_context {
    // Event library state and callbacks
    struct {
        void *data; // Event library internal data

        // Callbacks that are called when the library expects to start
        // reading/writing. These functions should be idempotent.
        riak_event_fn read_add;
        riak_event_fn read_remove;
        riak_event_fn write_add;
        riak_event_fn write_remove;
        riak_event_fn cleanup;
    } event;
    riak_result_fn result_fn;
    void *user_data;

    int err;
    char errstr[1024];
    riak_socket_t fd;  // Socket file descriptor
    riak_boolean_t done_reading;
    char* recv_buffer;
    char* send_buffer;
    int  recv_len;
    int  send_len;
} riak_context;

void riak_context_init(riak_context *ctx);
int riak_context_connect(riak_context *ctx, const char* host, const char* port);
int riak_send(riak_context *ctx, char *message, riak_result_fn fn);

void
riak_async_read_cb(riak_context *ctx);

void
riak_async_write_cb(riak_context *ctx);
