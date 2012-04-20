MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir) @DEFINES@

TESTS = test-getline test-buf test-proto test-proxy test-map

EXTRA_DIST = data/test1-1 data/test1-2 data/test1-3

check_PROGRAMS = $(TESTS)

test_getline_SOURCES = test_getline.c $(top_srcdir)/src/rpl_getline.h $(top_srcdir)/src/rpl_getline.x $(top_srcdir)/src/common.h $(top_srcdir)/src/compat.h $(top_srcdir)/src/compat.c

test_buf_SOURCES = test_buf.c $(top_srcdir)/src/buf.h $(top_srcdir)/src/buf.c $(top_srcdir)/src/common.h $(top_srcdir)/src/compat.h $(top_srcdir)/src/compat.c

test_proto_SOURCES = test_proto.c $(top_srcdir)/src/daemon_proto.h $(top_srcdir)/src/daemon_proto.c $(top_srcdir)/src/buf.h $(top_srcdir)/src/buf.c $(top_srcdir)/src/common.h $(top_srcdir)/src/compat.h $(top_srcdir)/src/compat.c

test_proxy_SOURCES = test_proxy.c $(top_srcdir)/src/http_proxy.h $(top_srcdir)/src/http_proxy.c $(top_srcdir)/src/buf.h $(top_srcdir)/src/buf.c $(top_srcdir)/src/common.h $(top_srcdir)/src/compat.h $(top_srcdir)/src/compat.c

test_map_SOURCES = test_map.c $(top_srcdir)/src/map.h $(top_srcdir)/src/map.c $(top_srcdir)/src/bitmap.h $(top_srcdir)/src/bitmap.c $(top_srcdir)/src/common.h $(top_srcdir)/src/compat.h $(top_srcdir)/src/compat.c
