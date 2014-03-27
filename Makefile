CC=gcc-4.8
CFLAGS = -std=c99 -I. `pkg-config libevent_pthreads --cflags` `pkg-config libuv --cflags`
LE_TARGET = test_libevent
LE_OBJ = test_libevent.o riak_async.o
LE_DEPS = test_libevent.c riak_async.c riak_async.h adapters/riak_libevent.h
LE_LIBS = `pkg-config libevent_pthreads --libs`
UV_TARGET = test_libuv
UV_OBJ = test_libuv.o riak_async.o
UV_DEPS = test_libuv.c riak_async.c riak_async.h adapters/riak_libuv.h
UV_LIBS = `pkg-config libuv --libs`

all: $(LE_TARGET) $(UV_TARGET)

$(LE_TARGET): $(LE_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LE_LIBS)

$(UV_TARGET): $(UV_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(UV_LIBS)

%.o: %.c $(LE_DEPS) $(UV_DEPS)
	$(CC) -g -c -o $@ $< $(CFLAGS)
	
clean:
	rm -f $(LE_OBJ) $(LE_TARGET) $(UV_OBJ) $(UV_TARGET)
