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

#include "sb.h"

session mt_session_create(mti ti, jpacket jp, char *user, char *pass, char *nick)
{
    pool p;
    session s;
    char *l;

    p = pool_new();
    s = pmalloc(p,sizeof(_session));
    s->p = p;
    s->ti = ti;
    s->q = mtq_new(p);
    s->buff = mt_jpbuf_new(p);
    lowercase(jp->from->server);
    s->id = jid_new(p,jp->from->server);
    lowercase(jp->from->user);
    jid_set(s->id,jp->from->user,JID_USER);
    s->host = pstrdup(s->p,jp->to->server);
    s->type = stype_normal;
    s->p_db = NULL;
    s->state = ustate_fln;
    s->st = NULL;
    s->users = NULL;
    s->rooms = NULL;
    s->chats = NULL;
    s->invites = NULL;
    s->user = pstrdup(p,user);
    s->pass = pstrdup(p,pass);
    s->nick = nick ? pstrdup(p, nick) : 0;
    s->friendly = 0;
    s->friendly_flag = 1;
    s->exit_flag = s->attempts = s->connected = 0;
    s->ref = 1;

    for(l = jid_full(s->id); *l != '\0'; l++)
        *l =  tolower(*l);

    xhash_put(ti->sessions,s->id->full,s);
    s->id->full = NULL;

    ++ti->sessions_count;

    mt_update_friendly(s, jp); /* it won't send the nick straight away  because friendly_flag is set*/

    log_debug(ZONE,"Created session for %s",jid_full(s->id));

    return s;
}

void mt_send_saved_friendly(session s)
{
    /* Send the temporary status & erase it */
    if(s->friendly_flag == 0) {
        if(s->friendly != 0) {
            mt_free(s->friendly);
        }
    }
    else {
        mt_cmd_rea(s->st,s->user,s->friendly);
        mt_free(s->friendly);
        s->friendly = 0;
        s->friendly_flag = 0;
    }
}

void mt_send_friendly(session s, pool p, char *friendly)
{
    if(friendly != 0 && strlen(friendly) >= 1) {
        if(strlen(friendly) >= 128) {
            friendly[122] = ' ';
            friendly[123] = '.';
            friendly[124] = '.';
            friendly[125] = '.';
            friendly[126] = '.';
            friendly[127] = '\0';
        }
        friendly = mt_encode(p, friendly);
    }
    else
        friendly = mt_encode(p, s->id->user);

    if(s->friendly_flag == 0) {
        log_debug(ZONE,"Sending MSN friendly name as %s for session %s", friendly, jid_full(s->id));
        mt_cmd_rea(s->st,s->user,friendly);
    }
    else {
        /* Save the status temporarily to be sent once we have received the contact list */
        log_debug(ZONE,"Saving MSN friendly name as '%s' for session %s", friendly, jid_full(s->id));
        s->friendly = mt_strdup(friendly);
    }
}

void mt_update_friendly(session s, jpacket jp)
{
    pool p = pool_new();
    char *status_msg = 0;
    spool friendly = spool_new(p);
    int len = 0;
    char *cfriendly = 0;

    /* Check to see if fancy nicks are enabled */
    if(s->ti->fancy_friendly == 0) {
        mt_send_friendly(s, p, s->nick);
        pool_free(p);
        return;
    }
    
    log_debug(ZONE,"Updating fancy friendly name for session %s",jid_full(s->id));

    /* Grab the status message from the presence packet and check it */
    status_msg = pstrdup(p, xmlnode_get_tag_data(jp->x,"status"));
    if(status_msg == 0 || strlen(status_msg) < 1) {
        /* We send just the nickname as the friendly name */
        mt_send_friendly(s,p,s->nick);
    }
    else
    /* Check the user entered nickname */
    if(s->nick == 0 || strlen(s->nick) > 127) {
        /* If we have a non-existant nickname setting, just send the status message */
        mt_send_friendly(s,p,status_msg);
    }
    else {
        /* Otherwise, we send a friendlyname in the format
        usernick - statusmsg
        */
        spool_add(friendly, s->nick);
        spool_add(friendly, " - ");
        spool_add(friendly, status_msg);
        cfriendly = pstrdup(p, spool_print(friendly));
        mt_send_friendly(s,p,cfriendly);
    }
    pool_free(p);
}

session mt_session_find(mti ti, jid id)
{
    char *full = jid_full(id);
    char buf[320];
    int i = 0;

    /* lower case and chop resource */
    for (i = 0; full[i] != '\0'; i++)
    {
        assert(i < 320);
        if (full[i] == '/')
            break;
        buf[i] = tolower(full[i]);
    }

    buf[i] = '\0';

    log_debug(ZONE,"Session lookup '%s'",buf);

    return (session) xhash_get(ti->sessions,buf);
}

void mt_session_connected(void *arg)
{
    session s = (session) arg;

    log_debug(ZONE,"Session[%s] connected",jid_full(s->id));

    if (s->exit_flag == 0)
    {
        if (s->chats == NULL)
        {
            s->chats = xhash_new(5);
            if (s->ti->con)
                s->rooms = xhash_new(5);
            if (s->ti->invite_msg)
                s->invites = xhash_new(5);
        }

        mt_jpbuf_flush(s->buff);
    }
    s->connected = 1;
}

void mt_session_handle(session s, jpacket jp)
{
    switch (jp->type)
    {
    case JPACKET_PRESENCE:
        mt_presence(s,jp);
        break;

    case JPACKET_S10N:
        mt_s10n(s,jp);
        break;

    case JPACKET_MESSAGE:
        mt_message(s,jp);
        break;

    case JPACKET_IQ:
        mt_iq(s,jp);
        break;
    }
}

void mt_session_process(session s, jpacket jp)
{
    mti ti = s->ti;

    log_debug(ZONE,"Session[%s] received packet, %d %d",jid_full(s->id),s->connected,s->exit_flag);

    if (s->exit_flag)
    {
        if (jp->type != JPACKET_PRESENCE || jpacket_subtype(jp) != JPACKET__UNAVAILABLE)
        {
            jutil_error(jp->x,TERROR_NOTFOUND);
            mt_deliver(ti,jp->x);
        }
        else
        {
            log_debug(ZONE,"Dropping packet");
            xmlnode_free(jp->x);
        }
    }
    else
    {
        SREF_INC(s);

        if (ti->con && j_strcmp(jp->to->server,ti->con_id) == 0)
            mt_con_handle(s,jp);
        else
            mt_session_handle(s,jp);

        SREF_DEC(s);
    }
}

void mt_session_exit(void *arg)
{
    session s = (session) arg;
    mti ti = s->ti;
    jpacket jp;
    char *l;

    log_debug(ZONE,"Session[%s], exiting",jid_full(s->id));

    for(l = jid_full(s->id); *l != '\0'; l++)
        *l =  tolower(*l);

    xhash_zap(ti->sessions,s->id->full);
    s->id->full = NULL;
    --ti->sessions_count;

    if (s->st != NULL)
    {
        if (s->connected)
            mt_cmd_out(s->st);

        mt_ns_close(s);
    }

    while ((jp = mt_jpbuf_de(s->buff)) != NULL)
    {
        jutil_error(jp->x,TERROR_NOTFOUND);
        mt_deliver(s->ti,jp->x);
    }

    mt_user_free(s);

    if (s->chats)
        xhash_free(s->chats);
    if (s->rooms)
        xhash_free(s->rooms);
    if (s->invites)
        xhash_free(s->invites);

    s->users = s->rooms = s->chats = s->invites = NULL;

    ppdb_free(s->p_db);
    s->p_db = NULL;
    SREF_DEC(s);
}

void mt_session_end(session s)
{
    if (s->exit_flag == 0)
    {
        log_debug(ZONE,"Ending session[%s]",jid_full(s->id));
        s->exit_flag = 1;
        mtq_send(s->q,s->p,&mt_session_exit,(void *) s);
    }
}

void mt_session_regerr(session s, terror e)
{
    jpacket jp;

    jp = mt_jpbuf_de(s->buff);
    assert(jp != NULL);
    jutil_error(jp->x,e);
    mt_deliver(s->ti,jp->x);
}

void mt_session_unavail(session s, char *msg)
{
    xmlnode pres;

    pres = jutil_presnew(JPACKET__UNAVAILABLE,jid_full(s->id),NULL);
    xmlnode_put_attrib(pres,"from",s->host);
    xmlnode_insert_cdata(xmlnode_insert_tag(pres,"status"),msg,-1);
    mt_deliver(s->ti,pres);
}

void mt_session_kill(session s, terror e)
{
    if (s->exit_flag == 0)
    {
        log_debug(ZONE,"Killing session[%s], error %s",jid_full(s->id),e.msg);
        s->exit_flag = 1;

        if (s->type == stype_register)
            mt_session_regerr(s,e);
        else
            mt_session_unavail(s,e.msg);

        mtq_send(s->q,s->p,&mt_session_exit,(void *) s);
    }
}
