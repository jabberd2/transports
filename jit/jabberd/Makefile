
include ../platform-settings

DEFINES=-DHOME="\"$(PREFIX)\""

CFLAGS:=$(COMMON_CFLAGS) $(DEFINES)

jabberd_HEADERS=jabberd.h

jabberd_OBJECTS = \
	config.o \
	mio.o \
	mio_raw.o \
	mio_xml.o \
	mio_ssl.o \
	deliver.o \
	heartbeat.o \
	jabberd.o \
	load.o \
	xdb.o \
	mtq.o \
	static.o \
	log.o \
	wplog.o \
	wphash.o

jabberd_XODEOBJECTS = \
	lib/expat.o \
	lib/genhash.o \
	lib/hashtable.o \
	lib/jid.o \
	lib/jpacket.o \
	lib/jutil.o \
	lib/karma.o \
	lib/pool.o \
	lib/pproxy.o \
	lib/rate.o \
	lib/sha.o \
	lib/snprintf.o \
	lib/socket.o \
	lib/str.o \
	lib/xmlnode.o \
	lib/xmlparse.o \
	lib/xmlrole.o \
	lib/xmltok.o \
	lib/xstream.o \
	lib/xhash.o

jabberd_BASEOBJECTS = \
	base/base_accept.o \
	base/base_connect.o \
	base/base_file.o \
	base/base_format.o \
	base/base_stderr.o \
	base/base_to.o

jabberd_WPextOBJECTS = 

jabberd_EXOBJECTS = $(jabberd_XODEOBJECTS) $(jabberd_BASEOBJECTS) 
jabberd_COMMONOBJS = $(jabberd_OBJECTS) $(jabberd_EXOBJECTS) $(jabberd_WPextOBJECTS)

all: dynamic

CFLAGS=$(COMMON_CFLAGS) $(DEFINES)
LIBS=$(COMMON_LIBS) 

dynamic: $(OUT_FILE)

$(OUT_FILE): $(jabberd_COMMONOBJS) $(JABBERD_EXPORTS_LIB)
	$(CC) -o $(OUT_FILE) $(JABBERD_EXPORTS_LIB) $(jabberd_COMMONOBJS) -Wl,--export-dynamic $(LIBS)

$(JABBERD_EXPORTS_LIB): $(jabberd_COMMONOBJS)
	dlltool --export-all-symbols --no-default-excludes \
		-e $(JABBERD_EXPORTS_LIB) \
		-l $(JABBERD_CYGWIN_LIB) \
		-D $(OUT_FILE) \
		$(jabberd_COMMONOBJS)


$(jabberd_COMMONOBJS):
	$(CC) -c $(CFLAGS) -o $@ $*.c

clean:
	rm -f *.a
	rm -f *~ *.o
	rm -f lib/*~ lib/*.o
	rm -f base/*~ base/*.o

distclean: clean
	rm -f $(OUT_FILE)

