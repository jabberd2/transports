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

result mt_presence_chg(mpacket mp, void *arg)
{
    if (j_strcmp(mt_packet_data(mp,0),"CHG"))
        mt_session_kill((session) arg,TERROR_EXTERNAL);

    return r_DONE;
}

void mt_presence(session s, jpacket jp)
{
    pool p = jp->p;

    log_debug(ZONE,"Session[%s], handling presence",jid_full(s->id));


    lowercase(jp->from->user);
    lowercase(jp->from->server);

    switch (jpacket_subtype(jp))
    {
    case JPACKET__AVAILABLE:
        /* ignore presence sent to users */
        if (jp->to->user == NULL)
        {
            ustate state;

            xmlnode_hide(xmlnode_get_tag(jp->x,"x"));
            s->p_db = ppdb_insert(s->p_db,jp->from,jp->x);
            state = mt_show2state(xmlnode_get_tag_data(jp->x,"show"));

            mt_update_friendly(s, jp); /* Update the friendly nickname */
            if (s->connected && state != s->state) /* Only send a CHG state command if our state actually changed */
            {
                mt_stream_register(s->st,&mt_presence_chg,(void *) s);
                mt_cmd_chg(s->st,mt_state2char(state));
            }

            s->state = state;
            xmlnode_put_attrib(jp->x,"from",jid_full(jp->to));
            xmlnode_put_attrib(jp->x,"to",jid_full(jid_user(jp->from)));
            mt_deliver(s->ti,jp->x);
            return;
        }
        break;

    case JPACKET__UNAVAILABLE:
        if (jp->to->user == NULL)
        {
            s->p_db = ppdb_insert(s->p_db,jp->from,jp->x);

            /* are there any available resources left? */
            if (ppdb_primary(s->p_db,s->id) == NULL)
            {
                xmlnode_put_attrib(jp->x,"to",jid_full(s->id));
                xmlnode_put_attrib(jp->x,"from",jid_full(jp->to));
                mt_deliver(s->ti,jp->x);
                mt_session_end(s);
                return;
            }
        }
        break;
    }

    xmlnode_free(jp->x);
}

/* handles available presence received from a jid without a session */
void mt_presence_unknown(void *arg)
{
    jpacket jp = (jpacket) arg;
    mti ti = (mti) jp->aux1;
    pool p = jp->p;
    xmlnode reg;
    session s;
    char *user, *nick, *pass;

    lowercase(jp->from->user);
    lowercase(jp->from->server);

    /* get account info from xdb */
    reg = xdb_get(ti->xc,mt_xdb_id(p,jp->from,jp->to->server),NS_REGISTER);
    if (reg == NULL)
    {
        /* they need to register first */
        jutil_error(jp->x,TERROR_REGISTER);
        mt_deliver(ti,jp->x);
        return;
    }

    /* hack to fix race condition.  prevents 2 available presence sent fast enough
       from starting 2 sessions */
    if ((s = mt_session_find(ti,jp->from)) != NULL)
    {
        log_debug(ZONE,"Session %s already created",jid_full(jp->from));
        xmlnode_free(reg);
        mt_presence(s,jp);
        return;
    }

    if ((user = xmlnode_get_tag_data(reg,"username")) == NULL ||
        (pass = xmlnode_get_tag_data(reg,"password")) == NULL)
    {
        log_error(ti->i->id,"Invalid XDB data");
        xmlnode_free(reg);
        jutil_error(jp->x,TERROR_INTERNAL);
        mt_deliver(ti,jp->x);
        return;
    }

    nick = xmlnode_get_tag_data(reg,"nick");
    s = mt_session_create(ti,jp,user,pass,nick);
    xmlnode_free(reg);

    xmlnode_hide(xmlnode_get_tag(jp->x,"x"));
    s->p_db = ppdb_insert(s->p_db,jp->from,jp->x);
    s->state = mt_show2state(xmlnode_get_tag_data(jp->x,"show"));

    /* starts the MSN connection and auth */
    mt_ns_start(s);

    xmlnode_put_attrib(jp->x,"from",jid_full(jp->to));
    xmlnode_put_attrib(jp->x,"to",jid_full(jid_user(jp->from)));
    mt_deliver(ti,jp->x);
}
