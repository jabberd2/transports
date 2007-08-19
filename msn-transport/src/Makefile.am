INCLUDES = -I$(includedir) -I$(top_builddir)/include -I. $(PTH_INC) $(JABBERD_INC) $(CURL_INC) -fPIC -shared

noinst_PROGRAMS = msntrans.so

msntrans_so_SOURCES = \
	init.c \
	chat.c \
	cmd.c \
	conf_room.c \
	conf_sb.c \
	iq.c \
	md5.c \
	ns.c \
	passport.c \
	presence.c \
	receive.c \
	register.c \
	s10n.c \
	session.c \
	stream.c \
	sync.c \
	user.c \
	utils.c \
	xhtml.c

msntrans_so_LDFLAGS = -fPIC -shared
CFLAGS = -Wall -g -O2 -Wno-unused
