SUBDIRS = jabber

bin_PROGRAMS = tt


if DEBUGSYM
tt_CFLAGS = -Wall -O2 -g
else
tt_CFLAGS = -Wall -O2
endif

tt_LDADD = ./jabber/libxmpp.a

tt_SOURCES = \
		all.h \
		tlen.c \
		tlen.h \
		jabber.c \
		jabber.h \
		jid.c \
		jid.h \
		users.c \
		users.h \
		encoding.h \
		encoding.c \
		enc_iso2uni.h \
		enc_uni2iso.h \
		debug.h \
		debug.c \
		tlenopt.h \
		main.c \
		config.c \
		config.h

EXTRA_DIST = Makefile.old
