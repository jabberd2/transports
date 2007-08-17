
#  $Id: jcomp.mk,v 1.5 2004/07/01 16:06:34 pcurtis Exp $

CFLAGS:=$(CFLAGS) -O2 -g -Wall -I../lib `pkg-config --cflags glib-2.0` -D_JCOMP -D_REENTRANT

LIBS:=$(LIBS) -L../lib -ljcomp -lm `pkg-config --libs glib-2.0` `pkg-config --libs gthread-2.0`

YAHOO_OBJECTS=yahoo-transport.o yahoo-session.o yahoo-phandler.o yahoo.o \
	yahoo-presence.o yahoo-server.o md5.o yahoo-message.o \
	yahoo-stats.o yahoo-composing.o yahoo-mail.o main.o \
	yahoo-auth.o gaim-sha.o crypt.o

all: yahoo-transport

yahoo-transport: $(YAHOO_OBJECTS)
	$(CC) $(CFLAGS) $(MCFLAGS) -o yahoo-transport $(YAHOO_OBJECTS) $(LDFLAGS) $(LIBS)

static: $(YAHOO_OBJECTS)

single: $(YAHOO_OBJECTS)

clean:
	rm -f $(YAHOO_OBJECTS) yahoo-transport.so



crypt.o: crypt.c md5.h
md5.o: md5.c md5.h
sha.o: sha.c sha.h
yahoo-message.o: yahoo-message.c yahoo-transport.h
yahoo-phandler.o: yahoo-phandler.c yahoo-transport.h
yahoo-presence.o: yahoo-presence.c yahoo-transport.h
yahoo-server.o: yahoo-server.c yahoo-transport.h
yahoo-session.o: yahoo-session.c yahoo-transport.h
yahoo-transport.o: yahoo-transport.c yahoo-transport.h
yahoo-composing.o: yahoo-composing.c yahoo-transport.h
yahoo-mail.o: yahoo-mail.c yahoo-transport.h
yahoo-stats.o: yahoo-stats.c yahoo-transport.h
yahoo.o: yahoo.c yahoo-transport.h
