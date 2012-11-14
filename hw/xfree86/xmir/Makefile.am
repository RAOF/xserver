INCLUDES =					\
	$(XORG_INCS)

libxmir_la_LTLIBRARIES = libxmir.la
libxmir_la_CFLAGS = \
	-DHAVE_XORG_CONFIG_H \
	$(DRI_CFLAGS) \
	$(DIX_CFLAGS) $(XORG_CFLAGS) $(LIBDRM_CFLAGS) \
	$(XMIR_CFLAGS) \
	-I$(top_srcdir)/hw/xfree86/common

libxmir_la_LDFLAGS = -module -avoid-version $(LIBDRM_LIBS) $(XMIR_LIBS)
libxmir_ladir = $(moduledir)/extensions
libxmir_la_SOURCES =			\
	xmir.c

sdk_HEADERS = xmir.h
