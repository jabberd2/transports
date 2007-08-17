#  $Id: Makefile,v 1.7 2004/06/25 18:33:56 pcurtis Exp $

CFLAGS:=$(CFLAGS) -I../jabberd -I../jabberd/lib `glib-config --cflags` -O2 -fPIC -Wall -D_REENTRANT
LIBS:=$(LIBS) `glib-config --libs glib` ../jabberd/lib/.libs/libjabberdlib.a

YAHOO_OBJECTS=yahoo-transport.o yahoo-session.o yahoo-phandler.o yahoo.o \
	crypt.o gaim-sha.o yahoo-presence.o yahoo-server.o md5.o yahoo-message.o \
	yahoo-stats.o yahoo-composing.o yahoo-mail.o yahoo-auth.o

all: yahoo-transport

yahoo-transport: $(YAHOO_OBJECTS)
	$(CXX) $(CFLAGS) $(MCFLAGS) -g -shared  -o yahoo-transport.so $(YAHOO_OBJECTS) $(LDFLAGS) $(LIBS)

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
yahoo-auth.o: yahoo-auth.c yahoo-auth.h
