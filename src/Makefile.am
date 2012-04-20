MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir) @DEFINES@ \
		   -DSYSCONFDIR='"$(sysconfdir)"' \
		   -DVERSION='"@VERSION@"'

bin_PROGRAMS = upnpproxy ssdp_mon

upnpproxy_SOURCES = daemon.c common.h \
                 ssdp.c ssdp.h \
				 selector.c selector.h \
				 timeval.c timeval.h \
				 socket.c socket.h \
				 http.c http.h \
				 buf.c buf.h \
				 cfg.c cfg.h \
				 log.c log.h \
				 util.c util.h \
				 daemon_proto.c daemon_proto.h \
				 map.c map.h \
				 bitmap.c bitmap.h \
				 vector.c vector.h \
				 timers.c timers.h \
				 compat.h compat.c rpl_getline.x \
				 http_proxy.h http_proxy.c

ssdp_mon_SOURCES = ssdp_mon.c common.h \
                   ssdp.c ssdp.h \
				   selector.c selector.h \
				   timeval.c timeval.h \
				   socket.c socket.h \
				   http.c http.h \
				   log.c log.h \
				   util.c util.h \
				   vector.c vector.h \
				   timers.c timers.h \
				   compat.h compat.c rpl_getline.x