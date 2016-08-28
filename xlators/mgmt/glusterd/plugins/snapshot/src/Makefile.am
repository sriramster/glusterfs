snapshot_LTLIBRARIES = libgsnaps.la
libgsnaps_la_SOURCES = lvm_snaps.c

libgsnaps_la_HEADERS = lvm_snaps.h

libgsnaps_ladir = $(includedir)/glusterfs/xlators/mgmt/plugins/snapshot/src/

libgsnaps_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

libgsnaps_la_LDFLAGS = -version-info $(LIBGSNAPS_LT_VERSION) $(GF_LDFLAGS)

snapshotdir = $(libdir)

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src \
	-I$(top_srcdir)/rpc/xdr/src \
	-I$(top_srcdir)/xlators/mgmt/glusterd/src \
	-I$(top_srcdir)/contrib/mount \
	-I$(top_srcdir)/contrib/uuid \
	-I$(top_srcdir)/rpc/rpc-lib/src \
	-I$(top_srcdir)/contrib/rbtree \
	-I$(top_srcdir)/rpc/xdr/src \
	-I/usr/include/libxml2 \
	-DCONFDIR=\"$(sysconfdir)/ganesha\" 

AM_CFLAGS = -Wall $(GF_CFLAGS)
AM_LDFLAGS = -L$(snapshotdir)

CLEANFILES = *~
