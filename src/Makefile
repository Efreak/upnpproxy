CC ?= gcc
CFLAGS = -Wall -g -O0 -DDEBUG -DHAVE_CONFIG_H -DVERSION='"0.1"' -DSYSCONFDIR='"/etc"'
LDFLAGS = -luuid

all: daemon ssdp_mon

clean:
	rm -f *.o daemon ssdp_mon

daemon: daemon.o ssdp.o selector.o timeval.o socket.o http.o buf.o cfg.o log.o util.o daemon_proto.o map.o vector.o timers.o compat.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

daemon.o: daemon.c common.h ssdp.h selector.h socket.h
	$(CC) $(CFLAGS) -c -o $@ $<

ssdp.o: ssdp.c common.h ssdp.h selector.h socket.h http.h util.h
	$(CC) $(CFLAGS) -c -o $@ $<

ssdp_mon: ssdp_mon.o ssdp.o selector.o timeval.o socket.o http.o util.o log.o vector.o timers.o compat.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

ssdp_mon.o: ssdp_mon.c common.h ssdp.h selector.h socket.h
	$(CC) $(CFLAGS) -c -o $@ $<

selector.o: selector.c selector.c common.h socket.h
	$(CC) $(CFLAGS) -c -o $@ $<

timeval.o: timeval.c timeval.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

socket.o: socket.c socket.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

http.o: http.c http.c socket.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

buf.o: buf.c buf.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

log.o: log.c log.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

cfg.o: cfg.c cfg.h log.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

util.o: util.c util.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

daemon_proto.o: daemon_proto.c daemon_proto.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

map.o: map.c map.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

vector.o: vector.c vector.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

timers.o: timers.c timers.h timeval.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<

compat.o: compat.c rpl_getline.c compat.h common.h
	$(CC) $(CFLAGS) -c -o $@ $<
