#include "riak_async.h"

void riak_context_init(riak_context *ctx) {
    ctx->err = 0;
    ctx->errstr[0] = '\0';
    ctx->user_data = NULL;
    ctx->result_fn = NULL;

    ctx->event.data = NULL;
    ctx->event.read_add = NULL;
    ctx->event.read_remove = NULL;
    ctx->event.write_add = NULL;
    ctx->event.write_remove = NULL;
    ctx->event.cleanup = NULL;

    // state of current read/write
    ctx->recv_len = 0;
    ctx->send_len = 0;
    ctx->send_buffer = NULL;
    ctx->recv_buffer = NULL;
    ctx->done_reading = RIAK_FALSE;
}

int riak_context_connect(riak_context *ctx,
        const char *host,
        const char *port) {

    riak_addrinfo    *addrinfo;
    riak_addrinfo addrhints;

    // Build the hints to tell getaddrinfo how to act.
    memset(&addrhints, '\0', sizeof(riak_addrinfo));
    //   addrhints.ai_family   = AF_UNSPEC; // IPv6 seems a bit broken for now
    addrhints.ai_family   = AF_INET; // IPv4 works like a champ
    addrhints.ai_socktype = SOCK_STREAM;
    addrhints.ai_protocol = IPPROTO_TCP; // We want a TCP socket
    /* Only return addresses we can use. */
    addrhints.ai_flags = AI_ADDRCONFIG;

    // Use nice, platform agnostic DNS lookup and return an array of results
    int err = getaddrinfo(host, port, &addrhints, &addrinfo);
    if (err != 0) {
        printf("Could not resolve the host\n");
        return err;
    }

    riak_socket_t sock = socket(addrinfo->ai_family,
                                   addrinfo->ai_socktype,
                                   addrinfo->ai_protocol);
    if (sock < 0) {
        printf("Could not just open a socket\n");
        return -1;
    }

#ifdef _RIAK_NON_BLOCKING
    riak_boolean_t blocking = RIAK_FALSE;
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
    if (fcntl(sock, F_SETFL, flags) != 0) {
       return -1;
    }
#endif

    err = connect(sock, addrinfo->ai_addr, addrinfo->ai_addrlen);
    if (err) {
        // Since this is nonblocking, we'll need to treat some errors
        // (like EINTR and EAGAIN) specially.
        if (errno != EINPROGRESS && errno != EAGAIN) {
            close(sock);
            printf("Could not connect a socket to host %s:%s [%s]\n", host, port, strerror(errno));
            return -1;
        }
    }

    ctx->fd = sock;

    return 0;
}

int riak_send(riak_context *ctx,
              char *message,
              riak_result_fn fn) {
    ctx->send_buffer = strdup(message);
    assert(ctx->send_buffer);
    memcpy(ctx->send_buffer, message, strlen(message));
    ctx->send_len = strlen(message);
    ctx->result_fn = fn;

    // Register a write event
    ctx->event.write_add(ctx->event.data);
    // Schedule a read for the response?
    // ctx->event.read_add(ctx->event.data);

    return 0;
}

int riak_buffer_read(riak_context *ctx) {
    char buf[1024*16];
    int nread;

    /* Return early when the context has seen an error. */
    if (ctx->err)
        return ctx->err;

    printf("READ block\n");
    nread = read(ctx->fd,buf,sizeof(buf));
    printf("READ %d bytes\n", nread);
    if (nread == -1) {
        if ((errno == EAGAIN) || (errno == EINTR)) {
            /* Try again later */
        } else {
            ctx->err = -1;
            return -1;
        }
    } else if (nread == 0) {
        // Server closed the connection
        ctx->err = 1;
        return 1;
    } else {
        // Things are looking rosy, so expand our growing message
        if (ctx->recv_buffer) {
            char *ptr = realloc(ctx->recv_buffer, (ctx->recv_len + nread + 1) * sizeof(char));
            assert(ptr);
            ctx->recv_buffer = ptr;
        } else {
            ctx->recv_buffer = malloc((nread + 1) * sizeof(char));
            assert(ctx->recv_buffer);
        }
        memcpy(&ctx->recv_buffer[ctx->recv_len], buf, nread);
        ctx->recv_len += nread;
        // Null-terminate for printing
        ctx->recv_buffer[ctx->recv_len] = '\0';
    }
    return 0;
}


int riak_buffer_write(riak_context *ctx, int *done) {
    int nwritten;

    /* Return early when the context has seen an error. */
    if (ctx->err)
        return ctx->err;

    if (ctx->send_len > 0) {
        nwritten = write(ctx->fd,ctx->send_buffer,ctx->send_len);
        printf("WROTE %d bytes\n", nwritten);
        if (nwritten == -1) {
            if ((errno == EAGAIN) || (errno == EINTR)) {
                /* Try again later */
            } else {
                ctx->err = -1;
                return -1;
            }
        } else if (nwritten > 0) {
            if (nwritten == ctx->send_len) {
                printf("WROTE ENTIRE ENCHILADA\n");
                ctx->send_len = 0;
            } else {
                // Move the output pointer (TBD)
                assert(0);
            }
        }
    }
    if (done != NULL) *done = (ctx->send_len == 0);
    return 0;
}


/* This function should be called when the socket is readable.
 * It processes all replies that can be read and executes their callbacks.
 */
void
riak_async_read_cb(riak_context *ctx) {

    int err = riak_buffer_read(ctx);
    if (err) {
        assert(0);
    } else {
        if (ctx->result_fn) ctx->result_fn(ctx);
        /* Always re-schedule reads, if not done */
        if (ctx->done_reading) {
            printf("riak_async_read_cb DONE\n");
            ctx->event.read_remove(ctx->event.data);
        } else {
            ctx->event.read_add(ctx->event.data);
        }
    }
}

void
riak_async_write_cb(riak_context *ctx) {
    int done = 0;

    int err = riak_buffer_write(ctx, &done);
    if (err) {
        assert(0);
    } else {
        /* Continue writing when not done, stop writing otherwise */
        if (!done) {
            printf("NOT DONE WRITING\n");
            ctx->event.write_add(ctx->event.data);
        } else {
            printf("DONE WRITING\n");
            free(ctx->send_buffer);
            ctx->send_buffer = NULL;
            ctx->event.write_remove(ctx->event.data);
        }

        /* Always schedule reads after writes */
        ctx->event.read_add(ctx->event.data);
    }
}
