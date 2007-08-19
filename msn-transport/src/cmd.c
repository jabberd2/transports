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

#include "stream.h"

void mt_cmd_ver(mpstream st)
{
    mt_stream_write(st,"VER %ld MSNP8 CVR0\r\n",st->trid);
}

void mt_cmd_cvr(mpstream st, char *user)
{
    mt_stream_write(st,"CVR %ld 0x0409 winnt 5.1 i386 MSNMSGR 5.0.0540 MSMSGS %s\r\n",st->trid, user);
}

void mt_cmd_usr(mpstream st, char *user, char *chalstr)
{
    mt_stream_write(st,"USR %ld %s %s\r\n",st->trid,user,chalstr);
}

void mt_cmd_usr_I(mpstream st, char *user)
{
    mt_stream_write(st,"USR %ld TWN I %s\r\n",st->trid,user);
}

void mt_cmd_usr_P(mpstream st, char *tp)
{
    mt_stream_write(st,"USR %ld TWN S %s\r\n",st->trid,tp);
}

void mt_cmd_syn(mpstream st)
{
    mt_stream_write(st,"SYN %ld 0\r\n",st->trid);
}

void mt_cmd_qry(mpstream st, char *result)
{
    mt_stream_write(st,"QRY %ld PROD0038W!61ZTF9 %d\r\n%s",st->trid, strlen(result), result);
}

void mt_cmd_chg(mpstream st, char *state)
{
    mt_stream_write(st,"CHG %ld %s\r\n",st->trid,state);
}

void mt_cmd_add(mpstream st, char *list, char *user, char *handle)
{
    mt_stream_write(st,"ADD %ld %s %s %s\r\n",st->trid,list,user,handle);
}

void mt_cmd_rem(mpstream st, char *list, char *user)
{
    mt_stream_write(st,"REM %ld %s %s\r\n",st->trid,list,user);
}

void mt_cmd_xfr_sb(mpstream st)
{
    mt_stream_write(st,"XFR %ld SB\r\n",st->trid);
}

void mt_cmd_cal(mpstream st, char *user)
{
    mt_stream_write(st,"CAL %ld %s\r\n",st->trid,user);
}

void mt_cmd_ans(mpstream st, char *user, char *chalstr, char *sid)
{
    mt_stream_write(st,"ANS %ld %s %s %s\r\n",st->trid,user,chalstr,sid);
}

void mt_cmd_msg(mpstream st, char *ack, char *msg)
{
    mt_stream_write(st,"MSG %ld %s %ld\r\n%s",st->trid,ack,strlen(msg),msg);
}

void mt_cmd_rea(mpstream st, char *user, char *nick)
{
    mt_stream_write(st,"REA %ld %s %s\r\n",st->trid,user,nick);
}

void mt_cmd_out(mpstream st)
{
    mt_stream_write(st,"OUT\r\n");
}
