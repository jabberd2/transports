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

#ifndef UTILS_H
#define UTILS_H

#include "msntrans.h"

typedef void (*jpbuf_cb)(jpacket jp, void *arg);

typedef struct jpnode_st
{
    jpacket jp;
    jpbuf_cb cb;
    void *arg;
    struct jpnode_st *next;
} *jpnode, _jpnode;

typedef struct jpbuf_st
{
    jpnode head, tail;
} *jpbuf, _jpbuf;

typedef enum
{
    ustate_nln = 0,
    ustate_fln,
    ustate_bsy,
    ustate_awy,
    ustate_phn,
    ustate_brb,
    ustate_idl,
    ustate_lun
} ustate;

typedef struct
{
    int b, i, u;
    char *font, *color;
    spool body;
} xhtml_msn;

#define for_each_node(c,n) for (cur = xmlnode_get_firstchild(n); cur != NULL; \
    cur = xmlnode_get_nextsibling(cur))

#define mt_deliver(ti,x) deliver(dpacket_new(x),ti->i)

#define should_escape(ch) (!(('A' <= ch && ch <= 'Z') || \
                             ('a' <= ch && ch <= 'z') || \
                             ('0' <= ch && ch <= '9')))

void lowercase(char *string);
char *mt_xhtml_format(xmlnode html);
void mt_xhtml_message(xmlnode message, char *fmt, char *text);

jid mt_mid2jid(pool p, char *mid, char *server);
char *mt_mid2jid_full(pool p, char *mid, char *server);
char *mt_jid2mid(pool p, jid id);

jpbuf mt_jpbuf_new(pool p);
void mt_jpbuf_en(jpbuf buf, jpacket jp, jpbuf_cb cb, void *arg);
jpacket mt_jpbuf_de(jpbuf buf);
void mt_jpbuf_flush(jpbuf buf);

char *mt_encode(pool p, char *buf);
char *mt_decode(pool p, char *buf);

ustate mt_show2state(char *show);
char *mt_state2char(ustate state);
ustate mt_char2state(char *state);

void mt_replace_newline(spool sp, char *str);
int mt_safe_user(char *user);
xmlnode mt_presnew(int type, char *to, char *from);
jid  mt_xdb_id(pool p, jid id, char *server);
void mt_md5hash(char *a, char *b, char result[33]);
char *mt_fix_amps(pool p, char *strIn);
int mt_is_entity(char *strIn);

#define mt_realloc realloc
#define mt_malloc  malloc
#define mt_strdup  strdup
#define mt_free    free

#endif
