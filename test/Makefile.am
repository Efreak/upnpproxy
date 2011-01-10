MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir) @DEFINES@

TESTS = test-getline test-buf

check_PROGRAMS = $(TESTS)

test_getline_SOURCES = test_getline.c $(top_srcdir)/src/rpl_getline.h $(top_srcdir)/src/rpl_getline.x $(top_srcdir)/src/common.h $(top_srcdir)/src/compat.h $(top_srcdir)/src/compat.c

test_buf_SOURCES = test_buf.c $(top_srcdir)/src/buf.h $(top_srcdir)/src/buf.c $(top_srcdir)/src/common.h $(top_srcdir)/src/compat.h $(top_srcdir)/src/compat.c
