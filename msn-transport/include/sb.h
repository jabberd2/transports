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

#ifndef SB_H
#define SB_H

#include "session.h"

typedef enum { sb_START, sb_READY, sb_CLOSE } sb_state;

typedef struct
{
    pool p;
    jid lid;
    char *mid;
    char *nick;
} *sbr_user, _sbr_user;

typedef struct
{
    pool p;
    session s;
    mpstream st;
    sb_state state;

    jid rid;
    char *name;
    char *uid, *nick;
    int legacy;
    xht users_mid;
    xht users_lid;
    int count;
} *sbroom, _sbroom;

typedef struct sbuser_st
{
    char *mid;
    struct sbuser_st *next;
} *sbc_user, _sbc_user;

typedef struct
{
    pool p;
    session s;
    mpstream st;
    sb_state state;

    jpbuf buff;
    char *thread;
    char *invite_id;
    int comp, lcomp_active, rcomp_counter;
    int xhtml;
    int count;
    sbc_user users;
} *sbchat, _sbchat;

void mt_message(session s, jpacket jp);
void mt_chat_join(session s, char *user, char *host, int port, char *chal, char *sid);
void mt_chat_message(session s, jpacket jp, char *to);
void mt_chat_end(sbchat sc);
void mt_chat_remove(sbchat sc);
void mt_chat_free(sbchat sc);
result mt_chat_docomposing(void *arg);

void mt_con_handle(session s, jpacket jp);
sbroom mt_con_create(session s, jid id, char *name, char *nick);
void mt_con_invite(session s, jpacket jp, char *user);
void mt_con_end(sbroom r);
void mt_con_remove(sbroom r);
void mt_con_free(sbroom r);
void mt_con_switch_mode(sbchat sc, jpacket jp, int legacy);

#define MIME_MSG "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nX-MMS-IM-Format: FN=MS%20Sans%20Serif; EF=; CO=0; CS=0; PF=0\r\n\r\n"
#define MIME_TYPE "MIME-Version: 1.0\r\nContent-Type: text/x-msmsgscontrol\r\nTypingUser: %s\r\n\r\n\r\n"

#endif
