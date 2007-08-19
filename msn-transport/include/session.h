/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "License").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the License.  You
 * may obtain a copy of the License at http://www.jabber.com/license/ or at
 * http://www.opensource.org/.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Copyright (c) 2000-2001 Schuyler Heath <sheath@jabber.org>
 *
 * Acknowledgements
 *
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 *
 * -------------------------------------------------------------------------- */

#ifndef SESSION_H
#define SESSION_H

#include "msntrans.h"
#include "stream.h"
#include "utils.h"

#define LIST_FL 0x01
#define LIST_RL 0x02
#define LIST_AL 0x04
#define LIST_BL 0x08

typedef struct muser_st
{
    ustate state;
    char *mid, *handle;
    int list, list_old;
} *muser, _muser;

typedef enum { stype_normal, stype_register } session_type;

typedef struct session_st
{
    pool p;
    mti ti;
    mtq q;
    jpbuf buff;
    jid id;
    char *host;
    session_type type;
    ppdb p_db;
    ustate state;
    mpstream st;
    xht users;
    xht rooms;
    xht chats;
    xht invites;
    char *user, *nick, *friendly, *pass;
    int exit_flag, attempts, connected, ref, friendly_flag;
    unsigned long int currentcontact, numcontacts;
} *session, _session;

#define SREF_INC(s) ++s->ref
#define SREF_DEC(s) if (--s->ref == 0){log_debug(ZONE,"freeing session %s %X",jid_full(s->id),s);pool_free(s->p);}

session mt_session_create(mti ti, jpacket jp, char *user, char *pass, char *nick);
session mt_session_find(mti ti, jid id);
void mt_session_connected(void *arg);
void mt_session_process(session s, jpacket jp);
void mt_session_unavail(session s, char *msg);
void mt_session_end(session s);
void mt_session_kill(session s, terror t);

void mt_update_friendly(session s, jpacket jp);
void mt_send_saved_friendly(session s);
void mt_presence(session s, jpacket jp);
void mt_presence_unknown(void *arg);
void mt_s10n(session s, jpacket jp);
void mt_iq(session s, jpacket jp);
void mt_iq_server(mti ti, jpacket jp);
void mt_iq_init(mti ti);
void mt_reg_unknown(void *arg);
void mt_reg_session(session, jpacket jp);
void mt_reg_success(void *arg);

void mt_ns_connect(session s, char *server, int port);
void mt_ns_start(session s);
void mt_ns_reconnect(session s);
void mt_ns_close(session s);

muser mt_user(session s, char *mid);
void mt_user_sync(session s);
void mt_user_update(session s, char *user, char *state, char *handle);
void mt_user_sendpres(session s, muser u);
void mt_user_subscribe(session s, muser u);
void mt_user_unsubscribe(session s, muser u);
void mt_user_free(session s);

void mt_ssl_auth(session s, char *authdata, char *tp);

#endif
