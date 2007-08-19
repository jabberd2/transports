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

muser mt_user_new(session s, char *mid)
{
    muser u;
    pool p = s->p;

    u = pmalloc(p,sizeof(_muser));
    u->state = ustate_fln;
    u->mid = pstrdup(p,mid);
    u->handle = mt_strdup(mid);
    *(strchr(u->handle,'@')) = '\0';
    u->list = 0;
    u->list_old = 0;

    xhash_put(s->users,u->mid,u);

    return u;
}

muser mt_user(session s, char *mid)
{
    muser u;

    assert(mid != NULL);

    u = (muser) xhash_get(s->users,mid);
    if (u == NULL)
        u = mt_user_new(s,mid);

    return u;
}

void mt_user_update(session s, char *user, char *state, char *handle)
{
    muser u = mt_user(s,user);

    if (handle && strcmp(u->handle,handle))
    {
        mt_free(u->handle);
        u->handle = mt_strdup(handle);
        mt_cmd_rea(s->st,user,handle);
    }

    u->state = mt_char2state(state);
    mt_user_sendpres(s,u);
}

void mt_user_sendpres(session s, muser u)
{
    ustate state = u->state;
    xmlnode x, status;
    jid from;
    pool p;
    char *handle;

    x = xmlnode_new_tag("presence");
    p = xmlnode_pool(x);

    from = mt_mid2jid(p,u->mid,s->host);
    xmlnode_put_attrib(x,"from",jid_full(from));
    xmlnode_put_attrib(x,"to",jid_full(s->id));

    if (state != ustate_fln)
    {
        handle = mt_decode(p,u->handle);
        status = xmlnode_insert_tag(x,"status");
        if (state != ustate_nln)
        {
            xmlnode show;

            show = xmlnode_insert_tag(x,"show");
            switch (state)
            {
            case ustate_bsy:
                xmlnode_insert_cdata(show,"dnd",-1);
                xmlnode_insert_cdata(status,spools(p,handle," (Busy)",p),-1);
                break;

            case ustate_awy:
                xmlnode_insert_cdata(show,"xa",-1);
                xmlnode_insert_cdata(status,spools(p,handle," (Away)",p),-1);
                break;

            case ustate_idl:
                xmlnode_insert_cdata(show,"xa",-1);
                xmlnode_insert_cdata(status,spools(p,handle," (Idle)",p),-1);
                break;

            case ustate_brb:
                xmlnode_insert_cdata(show,"away",-1);
                xmlnode_insert_cdata(status,spools(p,handle," (Be Right Back)",p),-1);
                break;

            case ustate_phn:
                xmlnode_insert_cdata(show,"away",-1);
                xmlnode_insert_cdata(status,spools(p,handle," (On The Phone)",p),-1);
                break;

            case ustate_lun:
                xmlnode_insert_cdata(show,"away",-1);
                xmlnode_insert_cdata(status,spools(p,handle," (Out To Lunch)",p),-1);
                break;

            default:
                break;
            }
        }
        else
            xmlnode_insert_cdata(status,handle,-1);
    }
    else
        xmlnode_put_attrib(x,"type","unavailable");

    mt_deliver(s->ti,x);
}

void _mt_user_subscribe(void *arg)
{
    xmlnode pres = (xmlnode) arg, roster;
    muser u;
    session s;
    jid id;
    pool p;

    p = xmlnode_pool(pres);
    s = (session) xmlnode_get_vattrib(pres,"s");
    u = (muser) xmlnode_get_vattrib(pres,"u");
    xmlnode_hide_attrib(pres,"s");
    xmlnode_hide_attrib(pres,"u");

    id = mt_xdb_id(p,s->id,s->host);
    roster = xdb_get(s->ti->xc,id,NS_ROSTER);

    if (roster == NULL)
    {
        roster = xmlnode_new_tag("query");
        xmlnode_put_attrib(roster,"xmlns",NS_ROSTER);
    }

    if (xmlnode_get_tag(roster,spools(p,"?jid=",u->mid,p)) == NULL)
    {
        xmlnode item = xmlnode_insert_tag(roster,"item");
        xmlnode_put_attrib(item,"jid",u->mid);
        xmlnode_put_attrib(item,"subscription","from");

        xdb_set(s->ti->xc,id,NS_ROSTER,roster);
    }

    xmlnode_free(roster);

    xmlnode_put_attrib(pres,"to",jid_full(s->id));
    xmlnode_put_attrib(pres,"from",mt_mid2jid_full(p,u->mid,s->host));
    xmlnode_put_attrib(pres,"type","subscribe");

    mt_deliver(s->ti,pres);
}

void mt_user_subscribe(session s, muser u)
{
    xmlnode pres;
    pres = xmlnode_new_tag("presence");
    xmlnode_put_vattrib(pres,"s",(void *) s);
    xmlnode_put_vattrib(pres,"u",(void *) u);
    mtq_send(s->q,xmlnode_pool(pres),&_mt_user_subscribe,(void *) pres);
}

void _mt_user_unsubscribe(void *arg)
{
    xmlnode pres = (xmlnode) arg, roster;
    muser u;
    session s;
    jid id;
    pool p;

    p = xmlnode_pool(pres);
    s = (session) xmlnode_get_vattrib(pres,"s");
    u = (muser) xmlnode_get_vattrib(pres,"u");
    xmlnode_hide_attrib(pres,"s");
    xmlnode_hide_attrib(pres,"u");

    id = mt_xdb_id(p,s->id,s->host);
    roster = xdb_get(s->ti->xc,id,NS_ROSTER);

    if (roster != NULL)
    {
        xmlnode item = xmlnode_get_tag(roster,spools(p,"?jid=",u->mid,p));
        if (item != NULL)
        {
            xmlnode_hide(item);
            xdb_set(s->ti->xc,id,NS_ROSTER,roster);
        }
        xmlnode_free(roster);
    }
    else
    {
        log_debug(ZONE,"Failed to remove user '%s', no roster found",u->mid);
    }

    xmlnode_put_attrib(pres,"to",jid_full(s->id));
    xmlnode_put_attrib(pres,"from",mt_mid2jid_full(p,u->mid,s->host));
    xmlnode_put_attrib(pres,"type","unsubscribe");

    mt_deliver(s->ti,pres);
}

void mt_user_unsubscribe(session s, muser u)
{
    xmlnode pres;
    pres = xmlnode_new_tag("presence");
    xmlnode_put_vattrib(pres,"s",(void *) s);
    xmlnode_put_vattrib(pres,"u",(void *) u);
    mtq_send(s->q,xmlnode_pool(pres),&_mt_user_unsubscribe,(void *) pres);
}

void _mt_user_free(xht h, const char *key, void *val, void *arg)
{
    muser u = (muser) val;
    session s = (session) arg;

    if (u->state != ustate_fln)
    {
        xmlnode pres;

        pres = jutil_presnew(JPACKET__UNAVAILABLE,jid_full(s->id),NULL);
        xmlnode_put_attrib(pres,"from",mt_mid2jid_full(pres->p,u->mid,s->host));
        mt_deliver(s->ti,pres);
    }

    mt_free(u->handle);
}

void mt_user_free(session s)
{
    if (s->users)
    {
        xhash_walk(s->users,&_mt_user_free,(void *) s);
        xhash_free(s->users);
    }
}
