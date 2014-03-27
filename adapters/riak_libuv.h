#ifndef __HIREDIS_LIBUV_H__
#define _RIAK_LIBUV_H_
#include <uv.h>
#include "../riak_async.h"


typedef struct _riak_uv_event {
  riak_context      *ctx;
  uv_poll_t          handle;
  int                events;
} riak_uv_event;


static void riak_libuv_poll(uv_poll_t* handle, int status, int events) {
  riak_uv_event* uvevent = (riak_uv_event*)handle->data;
  printf("riak_libuv_poll: status =%d, events=%d\n", status, events);

  if (status != 0) {
    return;
  }

  if (events & UV_READABLE) {
      riak_async_read_cb(uvevent->ctx);
  }
  if (events & UV_WRITABLE) {
      riak_async_write_cb(uvevent->ctx);
  }
}

static void riak_libuv_read_add(void *ptr) {
  riak_uv_event* uvevent = (riak_uv_event*)ptr;

  uvevent->events |= UV_READABLE;

  uv_poll_start(&uvevent->handle, uvevent->events, riak_libuv_poll);
}


static void riak_libuv_read_remove(void *ptr) {
  riak_uv_event* uvevent = (riak_uv_event*)ptr;

  uvevent->events &= ~UV_READABLE;

  if (uvevent->events) {
    uv_poll_start(&uvevent->handle, uvevent->events, riak_libuv_poll);
  } else {
    uv_poll_stop(&uvevent->handle);
  }
}


static void riak_libuv_write_add(void *ptr) {
  riak_uv_event* uvevent = (riak_uv_event*)ptr;

  uvevent->events |= UV_WRITABLE;

  uv_poll_start(&uvevent->handle, uvevent->events, riak_libuv_poll);
}


static void riak_libuv_write_remove(void *ptr) {
  riak_uv_event* uvevent = (riak_uv_event*)ptr;

  uvevent->events &= ~UV_WRITABLE;

  if (uvevent->events) {
    uv_poll_start(&uvevent->handle, uvevent->events, riak_libuv_poll);
  } else {
    uv_poll_stop(&uvevent->handle);
  }
}


static void on_close(uv_handle_t* handle) {
  riak_uv_event* uvevent = (riak_uv_event*)handle->data;

  free(uvevent);
}


static void riak_libuv_cleanup(void *ptr) {
  riak_uv_event* uvevent = (riak_uv_event*)ptr;

  uv_close((uv_handle_t*)&uvevent->handle, on_close);
}


int riak_libuv_init(riak_context *ctx, uv_loop_t* loop) {

  if (ctx->event.data != NULL) {
    return -1;
  }


  riak_uv_event* event = malloc(sizeof(riak_uv_event));

  if (!event) {
    return -1;
  }

  memset(event, 0, sizeof(riak_uv_event));

  if (uv_poll_init(loop, &event->handle, ctx->fd) != 0) {
    return -1;
  }

  /* Register functions to start/stop listening for events */
  ctx->event.read_add = riak_libuv_read_add;
  ctx->event.read_remove = riak_libuv_read_remove;
  ctx->event.write_add = riak_libuv_write_add;
  ctx->event.write_remove = riak_libuv_write_remove;
  ctx->event.cleanup = riak_libuv_cleanup;
  ctx->event.data = event;
  event->handle.data = event;
  event->ctx     = ctx;

  return 0;
}
#endif
