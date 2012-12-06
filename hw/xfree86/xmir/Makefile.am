INCLUDES =					\
	$(XORG_INCS)                            \
        -I$(srcdir)/../ddc                      \
        -I$(srcdir)/../ramdac                   \
        -I$(srcdir)/../i2c                      \
        -I$(srcdir)/../parser                   \
        -I$(srcdir)/../modes

libxmir_la_LTLIBRARIES = libxmir.la
libxmir_la_CFLAGS = \
	-DHAVE_XORG_CONFIG_H \
	$(DRI_CFLAGS) \
	$(DIX_CFLAGS) $(XORG_CFLAGS) $(LIBDRM_CFLAGS) \
	$(XMIR_CFLAGS)

libxmir_la_LDFLAGS = -module -avoid-version $(LIBDRM_LIBS) $(XMIR_LIBS)
libxmir_ladir = $(moduledir)/extensions
libxmir_la_SOURCES = \
	xmir.c \
	xmir-window.c

sdk_HEADERS = xmir.h
