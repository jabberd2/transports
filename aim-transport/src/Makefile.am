INCLUDES = -I$(includedir) -I. $(JABBERD_INC) -fPIC -shared

noinst_PROGRAMS = aimtrans.so

noinst_HEADERS = aim.h \
                 aim_cbtypes.h \
                 aim_internal.h \
                 aimtrans.h \
                 faimconfig.h \
                 iq.h \
                 libfaim_config.h \
                 md5.h \
                 utf8.h

aimtrans_so_SOURCES = \
  admin.c	\
  adverts.c	\
  auth.c	\
  bos.c	\
  buddies.c	\
  buddylist.c	\
  charset.c	\
  chat.c	\
  chatnav.c	\
  conn.c	\
  ft.c	\
  icq.c	\
  im.c	\
  info.c	\
  init.c	\
  invite.c	\
  iq.c	\
  iq_cb.c	\
  jabber_auth.c	\
  md5.c	\
  messages.c	\
  meta.c	\
  misc.c	\
  msgcookie.c	\
  msgconv.c     \
  parser.c	\
  popups.c	\
  pres.c	\
  register.c	\
  rxhandlers.c	\
  rxqueue.c	\
  s10n.c	\
  search.c	\
  service.c	\
  sessions.c	\
  snac.c	\
  ssi.c	\
  stats.c	\
  tlv.c	\
  translate.c	\
  txqueue.c	\
  util.c	\
  utils.c \
  utf8.c

aimtrans_so_LDFLAGS = -fPIC -shared 

CFLAGS += -DAIM_BUILDDATE=\"`date +%Y%m%d`\" -DAIM_BUILDTIME=\"`date +%H%M%S`\" -g
