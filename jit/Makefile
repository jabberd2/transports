
all:
	(cd jabberd && $(MAKE))
	(cd jit && $(MAKE))
	(cd xdb_file && $(MAKE))

clean:
	(cd xdb_file && $(MAKE) clean)
	(cd jit && $(MAKE) clean)
	(cd jabberd && $(MAKE) clean)

distclean:
	(cd xdb_file && $(MAKE) distclean)
	(cd jit && $(MAKE) distclean)
	(cd jabberd && $(MAKE) distclean)
	rm -f platform-settings

