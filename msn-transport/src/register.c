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

#include "session.h"

void mt_reg_get(mti ti, jpacket jp)
{
    xmlnode q, reg;

    lowercase(jp->from->server);
    lowercase(jp->from->user);

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x,"query");
    xmlnode_put_attrib(q,"xmlns",NS_REGISTER);

    reg = xdb_get(ti->xc,mt_xdb_id(jp->p,jp->from,jp->to->server),NS_REGISTER);
    if (reg != NULL && xmlnode_get_firstchild(reg) != NULL)
    {
        xmlnode_hide(xmlnode_get_tag(reg,"password"));
        xmlnode_hide(xmlnode_get_tag(reg,"key"));
        xmlnode_insert_node(q,xmlnode_get_firstchild(reg));
        xmlnode_insert_tag(q,"password");
        xmlnode_insert_tag(q,"registered");

        xmlnode_free(reg);
    }
    else
    {
        xmlnode_insert_tag(q,"username");
        xmlnode_insert_tag(q,"password");
        xmlnode_insert_tag(q,"nick");
    }

    xmlnode_insert_cdata(xmlnode_insert_tag(q,"key"),jutil_regkey(NULL,jid_full(jp->from)),-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"instructions"),ti->reg,-1);

    mt_deliver(ti,jp->x);
}

void mt_reg_remove(session s, jpacket jp)
{
    mti ti = s->ti;
    jid id;

    log_debug(ZONE,"Session[%s], unregistering",jid_full(s->id));

    id = mt_xdb_id(jp->p,s->id,s->host);

    xdb_set(ti->xc,id,NS_REGISTER,NULL);
    xdb_set(ti->xc,id,NS_ROSTER,NULL);

    mt_session_end(s);

    mt_deliver(s->ti,mt_presnew(JPACKET__UNSUBSCRIBE,jid_full(s->id),s->host));
    mt_session_unavail(s,"Unregistered");

    jutil_iqresult(jp->x);
    mt_deliver(ti,jp->x);
}

void mt_reg_success(void *arg)
{
    jpacket jp = (jpacket) arg;
    session s = (session) jp->aux1;
    mti ti = s->ti;
    pool p = jp->p;

    lowercase(jp->from->user);
    lowercase(jp->from->server);

    xmlnode_hide(xmlnode_get_tag(jp->iq,"instructions"));
    xmlnode_hide(xmlnode_get_tag(jp->iq,"key"));

    if (xdb_set(ti->xc,mt_xdb_id(p,s->id,s->host),NS_REGISTER,jp->iq) == 0)
    {
        if (ppdb_primary(s->p_db,s->id) == NULL)
        {
            xmlnode p;

            mt_deliver(ti,mt_presnew(JPACKET__SUBSCRIBE,jid_full(s->id),s->host));

            mt_session_end(s);

            p = xmlnode_new_tag("presence");
            xmlnode_put_attrib(p,"to",jid_full(jp->from));
            xmlnode_put_attrib(p,"from",jp->to->server);
            xmlnode_put_attrib(p,"type","probe");

            mt_deliver(ti,p);
        }
        else
            mt_user_sync(s);

        jutil_iqresult(jp->x);
    }
    else
        jutil_error(jp->x,TERROR_UNAVAIL);

    mt_deliver(ti,jp->x);
}

void mt_reg_update(session s, jpacket jp)
{
    mti ti = s->ti;
    char *user, *pass, *nick;
    int rcon = 0;

    log_debug(ZONE,"Updating registration");

    user = xmlnode_get_tag_data(jp->iq,"username");
    pass = xmlnode_get_tag_data(jp->iq,"password");
    nick = xmlnode_get_tag_data(jp->iq,"nick");

    /* Make sure it is a valid register query */
    if (user == NULL && pass == NULL && nick == NULL)
    { /* they have to be changing a least one setting */
        jutil_error(jp->x,TERROR_NOTACCEPTABLE);
        mt_deliver(ti,jp->x);
        return;
    }

    if (user != NULL && !mt_safe_user(user))
    {
        if (strchr(user,'@') == NULL)
            jutil_error(jp->x,(terror){406,"Username must be in the form user@hotmail.com!"});
        else
            jutil_error(jp->x,(terror){406,"Invalid Username"});
        mt_deliver(ti,jp->x);
        return;
    }

    if (user == NULL)
    {
        xmlnode_hide(xmlnode_get_tag(jp->iq,"username"));
        xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq,"username"),s->user,-1);
    }
    else if (j_strcasecmp(user,s->user) != 0)
    {
        s->user = pstrdup(s->p,user);
        rcon = 1;
    }

    if (pass == NULL)
    {
        xmlnode_hide(xmlnode_get_tag(jp->iq,"password"));
        xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq,"password"),s->pass,-1);
    }
    else if (j_strcmp(pass,s->pass) != 0)
    {
        s->pass = pstrdup(s->p,pass);
        rcon = 1;
    }

    if (nick != NULL)
    {
        char *tmp = mt_encode(jp->p,nick);
        if (j_strcmp(tmp,s->nick))
        {
            if (s->nick)
                mt_free(s->nick);
            s->nick = mt_strdup(tmp);
            if (rcon == 0) {
/*                mt_update_friendly(s, jp); */
                mt_cmd_rea(s->st, s->user, s->nick);
            }
        }
        else
            nick = NULL;
    }

    if (rcon)
    {
        /* if they changed their password or username then reconnect */
        assert(mt_jpbuf_de(s->buff) == NULL);
        mt_jpbuf_en(s->buff,jp,NULL,NULL);
        s->type = stype_register;
        s->connected = 0;
        s->friendly_flag = 1;
        mt_ns_close(s);
        mt_ns_start(s);
    }
    else
    {
        if (nick != NULL)
        {
            xmlnode_hide(xmlnode_get_tag(jp->iq,"instructions"));
            xmlnode_hide(xmlnode_get_tag(jp->iq,"key"));

            if (xdb_set(ti->xc,mt_xdb_id(jp->p,s->id,s->host),NS_REGISTER,jp->iq))
                jutil_error(jp->x,TERROR_UNAVAIL);
            else
                jutil_iqresult(jp->x);
        }
        else
        {
            log_debug(ZONE,"Settings unchanged");
            jutil_iqresult(jp->x);
        }

        mt_deliver(ti,jp->x);
    }
}

void mt_reg_session_set(void *arg)
{
    jpacket jp = (jpacket) arg;
    session s = (session) jp->aux1;

    if (xmlnode_get_tag(jp->iq,"remove") == NULL)
        mt_reg_update(s,jp);
    else
        mt_reg_remove(s,jp);
}

void mt_reg_session_set_flush(jpacket jp, void *arg)
{
    session s = (session) arg;

    if (xmlnode_get_tag(jp->iq,"remove") == NULL)
        mt_reg_update(s,jp);
    else
        mt_reg_remove(s,jp);
}

void mt_reg_session_get(void *arg)
{
    jpacket jp = (jpacket) arg;
    session s = (session) jp->aux1;
    mt_reg_get(s->ti,jp);
}

void mt_reg_session(session s, jpacket jp)
{
    switch (jpacket_subtype(jp))
    {
    case JPACKET__SET:
        if(s->connected)
        {
            jp->aux1 = (void *) s;
            mtq_send(s->q,jp->p,mt_reg_session_set,(void *) jp);
        }
        else
            mt_jpbuf_en(s->buff,jp,&mt_reg_session_set_flush,(void *) s);
        break;

    case JPACKET__GET:
        jp->aux1 = (void *) s;
        mtq_send(s->q,jp->p,&mt_reg_session_get,(void *) jp);
        break;

    default:
        jutil_error(jp->x,TERROR_BAD);
        mt_deliver(s->ti,jp->x);
        break;
    }
}

void mt_reg_new(mti ti, jpacket jp)
{
    char *user, *pass, *nick;

    user = xmlnode_get_tag_data(jp->iq,"username");
    pass = xmlnode_get_tag_data(jp->iq,"password");
    nick = xmlnode_get_tag_data(jp->iq,"nick");

    if (user == NULL || pass == NULL)
    {
        jutil_error(jp->x,TERROR_NOTACCEPTABLE);
        mt_deliver(ti,jp->x);
    }
    else if (!mt_safe_user(user))
    {
        if (strchr(user,'@') == NULL)
            jutil_error(jp->x,(terror){406,"Username must be in the form user@hotmail.com!"});
        else
            jutil_error(jp->x,(terror){406,"Invalid Username"});

        mt_deliver(ti,jp->x);
    }
    else
    {
        session s = mt_session_create(ti,jp,user,pass,nick);
        s->type = stype_register;
        mt_jpbuf_en(s->buff,jp,NULL,NULL);
        mt_ns_start(s);
    }
}

void mt_reg_unknown(void *arg)
{
    jpacket jp = (jpacket) arg;
    mti ti = (mti) jp->aux1;

    switch (jpacket_subtype(jp))
    {
    case JPACKET__SET:
        mt_reg_new(ti,jp);
        break;

    case JPACKET__GET:
        mt_reg_get(ti,jp);
        break;

    default:
        jutil_error(jp->x,TERROR_BAD);
        mt_deliver(ti,jp->x);
        break;
    }
}
