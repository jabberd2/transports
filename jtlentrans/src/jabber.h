#ifndef __JABBER_H
#define __JABBER_H

#include <libxode.h>

#include "users.h"

#include "jabber/message.h"
#include "jabber/presence.h"
#include "jabber/iq.h"
#include "jabber/login.h"
#include "jabber/logout.h"
#include "jabber/register.h"

#include "jabber/event.h"
#include "jabber/disco.h"

#include "jabber/gate.h"
#include "jabber/search.h"
#include "jabber/version.h"
#include "jabber/vcard.h"

extern int jabber_gniazdo;
extern int jabber_server;

extern struct sockaddr_in server_addr;
extern socklen_t server_addrlen;

extern struct servent *sp;
extern struct hostent *hp;

/* Buffor dla polaczenia */
extern char *js_buf;
extern int js_buf_len;
extern int js_buf_len_data;

extern char *js_buf2;
extern int js_buf2_len;
extern int js_buf2_n;

/* XStream */
extern xode_stream js;
extern xode_pool js_p;

/* Zmienne uzywane przez stream handlera */
extern char *sid;

extern int jabber_loged;

/* do monitorowania zdazen */

extern int jabber_gniazdo;

/* funkcje dostepne z zewnatrz */

extern int jabber_send(const char *buf);
extern int jabber_recv(char **buf);

extern int jabber_mypresence();
extern int jabber_ping();

extern int jabber_sendprobes();

extern int jabber_handler();
extern void jabber_stream_handler(int typ, xode x, void *arg);

extern void jabber_message_handler(xode x);
extern void jabber_presence_handler(xode x);
extern void jabber_iq_handler(xode x);
extern void jabber_chatcommands_handler(tt_user *user0, char *from, char *body);
extern char *get_field(xode query, const char *name);
extern char *get_x_field(xode xdata, const char *name);
extern void jabber_snderror(const char *from, const char *to, const char *type, const char *code, const char *msg, const char *id);
#endif /* __JABBER_H */
