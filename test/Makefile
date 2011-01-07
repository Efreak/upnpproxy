CC ?= gcc
CFLAGS = -Wall -g -O0 -DDEBUG -DHAVE_CONFIG_H -I../src
LDFLAGS =

all: run-tests

clean:
	rm -f *.o test-getline test-buf

run-tests: test-getline test-buf
	@for f in $^; do ./$$f; done

test-getline: test-getline.o compat.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-buf: test-buf.o buf.o compat.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-getline.o: test_getline.c ../src/common.h ../src/config.h ../src/rpl_getline.h ../src/rpl_getline.c
	$(CC) $(CFLAGS) -c -o $@ $<

test-buf.o: test_buf.c ../src/common.h ../src/config.h ../src/buf.h
	$(CC) $(CFLAGS) -c -o $@ $<

buf.o: ../src/buf.c ../src/buf.h ../src/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

compat.o: ../src/compat.c ../src/rpl_getline.c ../src/compat.h ../src/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

