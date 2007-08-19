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

result mt_s10n_add_fl(mpacket mp, void *arg)
{
    if (j_strcmp(mt_packet_data(mp,0),"ADD") == 0)
    {
        session s = (session) arg;
        muser u;
        xmlnode x;

        u = mt_user(s,mt_packet_data(mp,4));
        x = xmlnode_new_tag("presence");
        xmlnode_put_attrib(x,"to",jid_full(s->id));
        xmlnode_put_attrib(x,"from",mt_mid2jid_full(xmlnode_pool(x),u->mid,s->host));

        u->list |= LIST_FL;
        xmlnode_put_attrib(x,"type","subscribed");
        mt_deliver(s->ti,x);
        mt_user_sendpres(s,u);
    }
    else if (j_atoi(mt_packet_data(mp,0),0) == 0)
        return r_ERR;

    return r_DONE;
}

result mt_s10n_add_al(mpacket mp, void *arg)
{
    if (j_strcmp(mt_packet_data(mp,0),"ADD") == 0)
    {
        session s = (session) arg;
        muser u = mt_user(s,mt_packet_data(mp,4));
        u->list |= LIST_AL;
    }
    else if (j_atoi(mt_packet_data(mp,0),0) == 0)
        return r_ERR;

    return r_DONE;
}

result mt_s10n_rem_fl(mpacket mp, void *arg)
{
    if (j_strcmp(mt_packet_data(mp,0),"REM") == 0)
    {
        session s = (session) arg;
        muser u = mt_user(s,mt_packet_data(mp,4));
        jid from;

        u->list ^= LIST_FL;
        from = mt_mid2jid(mp->p,u->mid,s->host);
        mt_deliver(s->ti,mt_presnew(JPACKET__UNSUBSCRIBED,jid_full(s->id),jid_full(from)));

        if (u->state != ustate_fln)
        {
            u->state = ustate_fln;
            mt_deliver(s->ti,mt_presnew(JPACKET__UNAVAILABLE,jid_full(s->id),jid_full(from)));
        }
    }
    else if (j_atoi(mt_packet_data(mp,0),0) == 0)
        return r_ERR;

    return r_DONE;
}

result mt_s10n_rem_al(mpacket mp, void *arg)
{
    if (j_strcmp(mt_packet_data(mp,0),"REM") == 0)
    {
        session s = (session) arg;
        muser u = mt_user(s,mt_packet_data(mp,4));
        u->list ^= LIST_AL;
    }
    else if (j_atoi(mt_packet_data(mp,0),0) == 0)
        return r_ERR;

    return r_DONE;
}

result mt_s10n_rem_bl(mpacket mp, void *arg)
{
    if (j_strcmp(mt_packet_data(mp,0),"REM") == 0)
    {
        session s = (session) arg;
        muser u = mt_user(s,mt_packet_data(mp,4));
        u->list ^= LIST_BL;
    }
    else if (j_atoi(mt_packet_data(mp,0),0) == 0)
        return r_ERR;

    return r_DONE;
}

void mt_s10n_user(session s, jpacket jp, char *user)
{
    muser u;
    mpstream st = s->st;

    lowercase(jp->from->server);
    lowercase(jp->from->user);

    log_debug(ZONE,"Session[%s], handling subscription",jid_full(s->id));

    u = mt_user(s,user);

    switch (jpacket_subtype(jp))
    {
    case JPACKET__SUBSCRIBE:
        if (u->list & LIST_FL)
        {
            mt_deliver(s->ti,mt_presnew(JPACKET__SUBSCRIBED,jid_full(jp->from),jid_full(jp->to)));
            mt_user_sendpres(s,u);
        }
        else
        {
            mt_stream_register(st,&mt_s10n_add_fl,(void *) s);
            mt_cmd_add(st,"FL",user,u->handle);
        }
        break;

    case JPACKET__SUBSCRIBED:
        if (!(u->list & LIST_AL))
        {
            mt_stream_register(st,&mt_s10n_add_al,(void *) s);
            mt_cmd_add(st,"AL",user,u->handle);
        }

        /* if they are on the BL remove them */
        if (u->list & LIST_BL)
        {
            mt_stream_register(st,&mt_s10n_rem_bl,(void *) s);
            mt_cmd_rem(st,"BL",user);
        }
        break;

    case JPACKET__UNSUBSCRIBE:
        if (u->list & LIST_FL)
        {
            mt_stream_register(st,&mt_s10n_rem_fl,(void *) s);
            mt_cmd_rem(st,"FL",user);
        }
        else
        {
            mt_deliver(s->ti,mt_presnew(JPACKET__UNSUBSCRIBED,jid_full(jp->from),jid_full(jp->to)));
        }
        break;

    case JPACKET__UNSUBSCRIBED:
        if (u->list & LIST_AL)
        {
            mt_stream_register(st,&mt_s10n_rem_al,(void *) s);
            mt_cmd_rem(st,"AL",user);
        }
        break;
    }

    xmlnode_free(jp->x);
}

void mt_s10n_ready(jpacket jp, void *arg)
{
    session s = (session) arg;
    char *user = (char *) jp->aux1;
    mt_s10n_user(s,jp,user);
}

void mt_s10n_unsubscribe(void *arg)
{
    jpacket jp = (jpacket) arg;
    session s = (session) jp->aux1;
    mti ti = s->ti;
    jid id;

    id = mt_xdb_id(jp->p,s->id,s->host);

    xdb_set(ti->xc,id,NS_REGISTER,NULL);
    xdb_set(ti->xc,id,NS_ROSTER,NULL);

    mt_deliver(s->ti,mt_presnew(JPACKET__UNSUBSCRIBE,jid_full(s->id),s->host));
    mt_session_unavail(s,"Unregistered");
    mt_session_end(s);

    xmlnode_free(jp->x);
}

void mt_s10n_server(session s, jpacket jp)
{
    switch(jpacket_subtype(jp))
    {
    case JPACKET__SUBSCRIBE:
        jutil_tofrom(jp->x);
        xmlnode_put_attrib(jp->x, "type", "subscribed");
        xmlnode_hide(xmlnode_get_tag(jp->x, "status"));
        mt_deliver(s->ti, jp->x);
        return;

    case JPACKET__SUBSCRIBED:
        break;
    case JPACKET__UNSUBSCRIBE:
    case JPACKET__UNSUBSCRIBED:
        if (s->exit_flag == 0)
        {
            jp->aux1 = (void *) s;
            mtq_send(s->q,jp->p,&mt_s10n_unsubscribe,(void *) jp);
            return;
        }
        break;
    }

    xmlnode_free(jp->x);
}

void mt_s10n(session s, jpacket jp)
{
    char *user;

    if (jp->to->user == NULL)
    {
        mt_s10n_server(s,jp);
    }
    else if ((user =  mt_jid2mid(jp->p,jp->to)) == NULL || j_strcmp(user,s->user) == 0)
    {
        jutil_tofrom(jp->x);
        xmlnode_put_attrib(jp->x,"type","unsubscribed");
        xmlnode_hide(xmlnode_get_tag(jp->x,"status"));
        xmlnode_insert_cdata(xmlnode_insert_tag(jp->x,"status"),user ? "Invalid username" : "Not allowed",-1);
        mt_deliver(s->ti,jp->x);
    }
    else if (s->connected == 0)
    {
        jp->aux1 = (void *) user;
        mt_jpbuf_en(s->buff,jp,&mt_s10n_ready,(void *) s);
    }
    else
        mt_s10n_user(s,jp,user);
}
