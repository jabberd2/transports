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

#ifndef STREAM_H
#define STREAM_H

#include "msntrans.h"

typedef struct mpacket_st
{
    pool p;
    char **params;
    int count;
} *mpacket, _mpacket;

typedef result (*handle)(mpacket mp, void *arg);

typedef struct mphandler_st
{
    handle cb;
    void *arg;
    unsigned long trid;
    struct mphandler_st *next;
} *mphandler, _mphandler;

typedef struct mpstream_st
{
    mio m;

    handle cb;
    void *arg;
    mphandler head, tail;

    unsigned long trid;
    int closed;

    mpacket mp;
    char *buffer;
    int bufsz, msg_len;
} *mpstream, _mpstream;

void mt_stream_init();
mpstream mt_stream_connect(char *host, int port, handle cb, void *arg);
void mt_stream_register(mpstream st, handle cb, void *arg);
void mt_stream_write(mpstream st, const char *fmt, ...);
void mt_stream_close(mpstream st);

result mt_user_syn(mpacket mp, void *arg);
void mt_cmd_ver(mpstream st);
void mt_cmd_cvr(mpstream st, char *user);
void mt_cmd_usr(mpstream st, char *user, char *chalstr);
void mt_cmd_usr_I(mpstream st, char *user);
void mt_cmd_usr_P(mpstream st, char *tp);
void mt_cmd_syn(mpstream st);
void mt_cmd_qry(mpstream st, char *result);
void mt_cmd_chg(mpstream st, char *state);
void mt_cmd_add(mpstream st, char *list, char *user, char *handle);
void mt_cmd_rem(mpstream st, char *list, char *user);
void mt_cmd_xfr_sb(mpstream st);
void mt_cmd_cal(mpstream st, char *user);
void mt_cmd_ans(mpstream st, char *user, char *chalstr, char *sid);
void mt_cmd_msg(mpstream st, char *ack, char *msg);
void mt_cmd_rea(mpstream st, char *user, char *nick);
void mt_cmd_out(mpstream st);

#define mt_packet_data(mp,pos) mp->count > pos ? mp->params[pos] : NULL

#endif
