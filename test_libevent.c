
//#include "riak_async.h"
#include "adapters/riak_libevent.h"

void
result_fn(void *ptr) {
    riak_context *ctx = (riak_context*)ptr;
    char *buf = ctx->recv_buffer;
    char *lenptr = strstr(ctx->recv_buffer, "Content-Length: ");
    int delta = strlen("Content-Length: ");
    // Stoopid parser for HTTP header
    if (lenptr) {
        char *start = lenptr + delta;
        char *end = start;
        for(; *end >= '0' && *end <= '9'; end++);
        char oldval = *end;
        *end = '\0';
        int total = atoi(start);
        *end = oldval;
        // Have we read at least this many bytes? If so, we're done
        if (ctx->recv_len > total) {
            printf("RESULT: %d\n%s\n",  ctx->recv_len, ctx->recv_buffer);
            ctx->done_reading = RIAK_TRUE;
        }
    }
}

int main (int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

    event_enable_debug_mode();
    int result = evthread_use_pthreads();
    if (result < 0) {
        printf("Could not use pthreads with libevent\n");
        exit(1);
    }

    struct event_base *base = event_base_new();
    if (base == NULL) {
        printf("Could not create libevent base\n");
        exit(1);
    }
    result = evthread_make_base_notifiable(base);
    if (result) {
        printf("Could not initialize libevent base\n");
        exit(1);
    }

    riak_context ctx_data[10];
    for(int i = 0; i < 10; i++) {

        riak_context *ctx = &ctx_data[i];
        riak_context_init(ctx);
        if (ctx->err) {
            printf("Error: %s\n", ctx->errstr);
            return 1;
        }
        result = riak_context_connect(ctx, "httpbin.org", "80");
    //    result = riak_context_connect(ctx, "localhost", "6074");
        if (result) {
            printf("Could not connect to host\n");
            exit(1);
        }
        result = riak_libevent_init(ctx, base);
        if (result) {
            printf("Could not initialize libevent\n");
            exit(1);
        }
        riak_send(ctx, "GET / HTTP/1.1\r\nHost: httpbin.org\r\n\r\n", result_fn);
    }
    event_base_dispatch(base);
    // cleanup
    event_base_free(base);
    return 0;
}
