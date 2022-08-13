plugin_LTLIBRARIES = libgstprrtsrc.la libgstprrtsink.la

# PRRT directory
prrt_dir=$(word 1, $(subst /plugin/, ,$(dir $(abspath $(lastword $(MAKEFILE_LIST))))))

LOCATION_1 = $(prrt_dir)/PRRT/build/libprrt.a
#AC_SUBST(LOCATION_1)
LOCATION_2 = $(prrt_dir)/PRRT/build/libprrtUtil.a
#AC_SUBST(LOCATION_2)

# plugin sources
libgstprrtsrc_la_SOURCES = gstprrtsrc.c
libgstprrtsink_la_SOURCES = gstprrtsink.c

# flags set in configure.ac
libgstprrtsrc_la_CFLAGS = $(GST_CFLAGS)
libgstprrtsrc_la_LIBADD = $(GST_LIBS) $(LOCATION_1) $(LOCATION_2)
libgstprrtsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstprrtsrc_la_LIBTOOLFLAGS = --tag=disable-static

libgstprrtsink_la_CFLAGS = $(GST_CFLAGS)
libgstprrtsink_la_LIBADD = $(GST_LIBS) $(LOCATION_1) $(LOCATION_2)
libgstprrtsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstprrtsink_la_LIBTOOLFLAGS = --tag=disable-static

# do not install these headers
noinst_HEADERS = gstprrtsrc.h gstprrtsink.h
