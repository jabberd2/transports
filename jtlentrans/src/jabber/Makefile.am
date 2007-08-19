INCLUDES=-I../

noinst_LIBRARIES = libxmpp.a


if DEBUGSYM
libxmpp_a_CFLAGS = -Wall -O2 -g
else
libxmpp_a_CFLAGS = -Wall -O2
endif

libxmpp_a_SOURCES = \
		message.c \
		message.h \
		iq.c \
		iq.h \
		presence.c \
		presence.h \
		login.c \
		login.h \
		logout.c \
		logout.h \
		register.c \
		register.h \
		event.c \
		event.h \
		search.c \
		search.h \
		vcard.c \
		vcard.h \
		version.c \
		version.h \
		gate.c \
		gate.h \
		disco.c \
		disco.h
