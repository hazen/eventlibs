#ifndef _RIAK_LIBEVENT_H_
#define _RIAK_LIBEVENT_H_
#include <event.h>
#include <event2/thread.h>
#include "../riak_async.h"

typedef struct _riak_libevent_events{
    riak_context *ctx;
    struct event read_event;
    struct event write_event;
} riak_libevent_events;

static void riak_libevent_read_cb(riak_socket_t fd, short event, void *arg) {
    ((void)fd); ((void)event);
    riak_libevent_events *e = (riak_libevent_events*)arg;
    riak_async_read_cb(e->ctx);
}

static void riak_libevenet_write_cb(riak_socket_t fd, short event, void *arg) {
    ((void)fd); ((void)event);
    riak_libevent_events *e = (riak_libevent_events*)arg;
    riak_async_write_cb(e->ctx);
}

static void riak_libevent_read_add(void *ptr) {
    riak_libevent_events *e = (riak_libevent_events*)ptr;
    event_add(&e->read_event,NULL);
}

static void riak_libevent_read_remove(void *ptr) {
    riak_libevent_events *e = (riak_libevent_events*)ptr;
    event_del(&e->read_event);
}

static void riak_libevent_write_add(void *ptr) {
    riak_libevent_events *e = (riak_libevent_events*)ptr;
    event_add(&e->write_event,NULL);
}

static void riak_libevent_write_remove(void *ptr) {
    riak_libevent_events *e = (riak_libevent_events*)ptr;
    event_del(&e->write_event);
}

static void riak_libevent_cleanup(void *ptr) {
    riak_libevent_events *e = (riak_libevent_events*)ptr;
    event_del(&e->read_event);
    event_del(&e->write_event);
    free(e);
}

int
riak_libevent_init(riak_context *ctx,
                   struct event_base *base) {

    /* Nothing should be attached when something is already attached */
    if (ctx->event.data != NULL) {
        printf("Looks like an event is already configured\n");
        return -1;
    }

    /* Create container for context and r/w events */
    riak_libevent_events *event = (riak_libevent_events*)malloc(sizeof(riak_libevent_events));
    event->ctx = ctx;

    /* Register functions to start/stop listening for events */
    ctx->event.read_add = riak_libevent_read_add;
    ctx->event.read_remove = riak_libevent_read_remove;
    ctx->event.write_add = riak_libevent_write_add;
    ctx->event.write_remove = riak_libevent_write_remove;
    ctx->event.cleanup = riak_libevent_cleanup;
    ctx->event.data = event;

    /* Initialize and install read/write events */
    event_set(&event->read_event,ctx->fd,EV_READ,riak_libevent_read_cb,event);
    event_set(&event->write_event,ctx->fd,EV_WRITE,riak_libevenet_write_cb,event);
    event_base_set(base,&event->read_event);
    event_base_set(base,&event->write_event);
    return 0;
}
#endif
