
include ../platform-settings

jabberd_HEADERS=

module_HEADERS=

module_MODULES=xdb_file.o \
	xdb_utils.o \
	xdb_hash.o

module_LIB=$(XDB_MODULE)

LIBS=$(COMMON_LIBS)
CFLAGS=$(COMMON_CFLAGS) $(module_CFLAGS)

all: dynamic

dynamic: $(module_LIB)

$(module_LIB): $(module_MODULES)
	$(CC) -shared -o $(module_LIB) $(module_MODULES) $(LIBS) 

clean:
	rm -f *~ $(module_MODULES) xdb_path.o xdb_copy.o

distclean: clean
	rm -f $(module_LIB) xdb_path xdb_copy

path: xdb_path.o xdb_hash.o
	$(CC) -o xdb_path xdb_path.o xdb_hash.o

copy: xdb_copy.o xdb_hash.o
	$(CC) -o xdb_copy xdb_copy.o xdb_hash.o

