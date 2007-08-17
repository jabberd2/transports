
include ../platform-settings

all: dynamic

jit_MODULE=$(JIT_MODULE)

jit_MODULES=jit/icqtransport.o \
	jit/unknown.o \
	jit/iq.o \
	jit/session.o \
	jit/message.o \
	jit/presence.o \
	jit/contact.o \
	utils/charset.o \
	utils/xdata.o \
	utils/utils.o 

jit_LIB=jit/server.o \
	jit/wp_client.o \
	src/Capabilities.o \
	src/Client.o \
	src/Contact.o \
	src/ContactList.o \
	src/DirectClient.o \
	src/ICBMCookie.o \
	src/ICQ.o \
	src/MessageHandler.o \
	src/SNAC-BOS.o \
	src/SNAC-BUD.o \
	src/SNAC-GEN.o \
	src/SNAC-LOC.o \
	src/SNAC-MSG.o \
	src/SNAC-SBL.o \
	src/SNAC-SRV.o \
	src/SNAC-UIN.o \
	src/SNAC-base.o \
	src/SNAC.o \
	src/TLV.o \
	src/Translator.o \
	src/UserInfoBlock.o \
	src/Xml.o \
	src/buffer.o \
	src/time_extra.o \
	src/exceptions.o \
	src/events.o \
	src/version.o


LIBS=$(COMMON_LIBS)
CFLAGS=$(COMMON_CFLAGS)
CXXFLAGS=$(CFLAGS)

dynamic: $(jit_MODULE)

$(jit_MODULE): $(jit_MODULES) $(jit_LIB)
	$(CXX) -shared -o $(jit_MODULE) $(jit_MODULES) $(jit_LIB) $(LIBS) 

$(jit_MODULES): jit/icqtransport.h jit/wp_client.h utils/polish.h utils/english.h
$(jit_LIB): libicq2000/Client.h
server.o: libicq2000/Client.h
wp_client.o: libicq2000/Client.h

clean: 
	rm -f $(jit_MODULES) $(jit_LIB) *~ libicq2000/*~ src/*~ utils/*~ jit/*~

distclean: clean
	rm -f $(jit_MODULE)

dclean: distclean

