lib_LTLIBRARIES = libgsnapshot.la

libgsnapshot_la_SOURCES = lvm-snapshot.c

libgsnapshot_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la
libgsnapshot_la_LDFLAGS = -version-info $(LIBGSNAPS_LT_VERSION) $(GF_LDFLAGS)

libgsnapshot_la_HEADERS = lvm-snapshot.h

libgsnapshot_ladir = $(includedir)/glusterfs/rpc

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src \
	-I$(top_srcdir)/xlators/mgmt/glusterd/src \
	-I$(top_srcdir)/rpc/xdr/src \
	-I$(top_srcdir)/contrib/rbtree

AM_CFLAGS = -Wall $(GF_CFLAGS)

CLEANFILES = *~
