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

void mt_con_send(sbroom r, jpacket jp)
{
    spool sp;
    char *body, *msg;

    body = xmlnode_get_tag_data(jp->x,"body");
    sp = spool_new(jp->p);
    spool_add(sp,MIME_MSG);
    mt_replace_newline(sp,body);
    msg = spool_print(sp);

    mt_cmd_msg(r->st,"U",msg);

    /* echo the message back */
    jutil_tofrom(jp->x);
    xmlnode_put_attrib(jp->x,"from",r->uid);

    mt_deliver(r->s->ti,jp->x);
}

void mt_con_message(session s, jpacket jp)
{
    mti ti = s->ti;
    sbroom r;

    lowercase(jp->to->user);
    r = (sbroom) xhash_get(s->rooms,jp->to->user);

    if (r != NULL && r->state == sb_READY)
    {
        if (jp->to->resource == NULL)
        {
            if (xmlnode_get_tag_data(jp->x,"body") == NULL)
            {
                jutil_error(jp->x,TERROR_NOTALLOWED);
                mt_deliver(ti,jp->x);
            }
            else
                mt_con_send(r,jp);
        }
        else
        {
            sbr_user user = xhash_get(r->users_lid,jp->to->resource);

            if (user == NULL)
            {
                jutil_error(jp->x,TERROR_NOTFOUND);
                mt_deliver(s->ti,jp->x);
            }
            else
                mt_chat_message(s,jp,user->mid);
        }
    }
    else
    {
        jutil_error(jp->x,TERROR_NOTFOUND);
        mt_deliver(s->ti,jp->x);
    }
}

result mt_con_cal(mpacket mp, void *arg)
{
    if (j_strcmp(mt_packet_data(mp,0),"CAL"))
    {
        if (j_atoi(mt_packet_data(mp,0),0) == 0)
            return r_ERR;
    }
    return r_DONE;
}

void mt_con_invite(session s, jpacket jp, char *user)
{
    sbroom r;
    char *rid, *ptr;

    if (s->connected == 0)
    {
        jutil_error(jp->x,TERROR_NOTFOUND);
        mt_deliver(s->ti,jp->x);
        return;
    }

    rid = pstrdup(jp->p,xmlnode_get_attrib(xmlnode_get_tag(jp->x,"x"),"jid"));
    if (rid == NULL || (ptr = strchr(rid,'@')) == NULL)
    {
        jutil_error(jp->x,TERROR_BAD);
        mt_deliver(s->ti,jp->x);
        return;
    }
    *ptr = '\0';

    lowercase(rid);
    r = (sbroom) xhash_get(s->rooms,rid);
    if (r != NULL && r->state == sb_READY)
    {
        if (xhash_get(r->users_mid,user) == NULL)
        {
            mt_stream_register(r->st,&mt_con_cal,(void *) r);
            mt_cmd_cal(r->st,user);
        }
        xmlnode_free(jp->x);
    }
    else
    { /* you can't invite ppl into a chat you've yet to join */
        jutil_error(jp->x,TERROR_NOTFOUND);
        mt_deliver(s->ti,jp->x);
    }
}

void mt_con_browse_server_walk(xht h, const char *key, void *val, void *arg)
{
    sbroom r = (sbroom) val;
    xmlnode q = (xmlnode) arg;
    xmlnode x;
    char buf[3];

    x = xmlnode_insert_tag(q,"conference");
    xmlnode_put_attrib(x,"type","private");
    xmlnode_put_attrib(x,"jid",jid_full(r->rid));
    snprintf(buf,3,"%d",r->count + 1);
    xmlnode_put_attrib(x,"name",spools(xmlnode_pool(q),r->name," (",buf,")",xmlnode_pool(q)));
}

void mt_con_disco_server_walk(xht h, const char *key, void *val, void *arg)
{
    sbroom r = (sbroom) val;
    xmlnode q = (xmlnode) arg;
    xmlnode item;
    char buf[3];

    item = xmlnode_insert_tag(q, "item");
    xmlnode_put_attrib(item, "jid", jid_full(r->rid));

    /* the number of users in this room */
    snprintf(buf, 3, "%d", r->count+1);
    xmlnode_put_attrib(item, "name", spools(xmlnode_pool(q), r->name, " (",buf, ")", xmlnode_pool(q)));
}

void mt_con_browse_server(session s, jpacket jp)
{
    if (jpacket_subtype(jp) == JPACKET__GET)
    {
        xmlnode q;

        jutil_iqresult(jp->x);
        q = xmlnode_insert_tag(jp->x,"conference");
        xmlnode_put_attrib(q,"xmlns",NS_BROWSE);
        xmlnode_put_attrib(q,"name","MSN Conference");
        xmlnode_put_attrib(q,"type","private");

        xhash_walk(s->rooms,&mt_con_browse_server_walk,(void *) q);
    }
    else
        jutil_error(jp->x,TERROR_BAD);

    mt_deliver(s->ti,jp->x);
}

void mt_con_disco_items_server(session s, jpacket jp)
{
    xmlnode q;

    if (jpacket_subtype(jp) != JPACKET__GET)
    {
	jutil_error(jp->x,TERROR_BAD);
	mt_deliver(s->ti, jp->x);
	return;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_ITEMS);

    xhash_walk(s->rooms,&mt_con_disco_server_walk,(void *) q);

    mt_deliver(s->ti, jp->x);
}

void mt_con_disco_info_server(session s, jpacket jp)
{
    xmlnode q, info;

    if (jpacket_subtype(jp) != JPACKET__GET)
    {
	jutil_error(jp->x, TERROR_BAD);
	mt_deliver(s->ti, jp->x);
	return;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_INFO);

    info = xmlnode_insert_tag(q, "identity");
    xmlnode_put_attrib(info, "category", "conference");
    xmlnode_put_attrib(info, "type", "text");
    xmlnode_put_attrib(info, "name","MSN Conference");

    mt_deliver(s->ti, jp->x);
}

void mt_con_browse_user(sbroom r, jpacket jp)
{
    sbr_user user;

    user = (sbr_user) xhash_get(r->users_lid,jp->to->resource);
    if (user != NULL)
    {
        xmlnode u, m;

        jutil_iqresult(jp->x);
        u = xmlnode_insert_tag(jp->x,"user");
        xmlnode_put_attrib(u,"xmlns",NS_BROWSE);
        xmlnode_put_attrib(u,"name",user->nick);

        m = xmlnode_insert_tag(u,"user");
        xmlnode_put_attrib(m,"jid",mt_mid2jid_full(jp->p,user->mid,r->s->host));
        xmlnode_put_attrib(m,"name",user->nick);
    }
    else
        jutil_error(jp->x,TERROR_NOTFOUND);
}

void mt_con_disco_items_user(sbroom r, jpacket jp)
{
    sbr_user user;
    xmlnode q;

    user = (sbr_user) xhash_get(r->users_lid, jp->to->resource);
    if (user == NULL)
    {
	jutil_error(jp->x,TERROR_BAD);
	return;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_ITEMS);
}

void mt_con_disco_info_user(sbroom r, jpacket jp)
{
    sbr_user user;
    xmlnode q, info;

    user = (sbr_user) xhash_get(r->users_lid, jp->to->resource);
    if (user == NULL)
    {
	jutil_error(jp->x,TERROR_BAD);
	return;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_INFO);

    info = xmlnode_insert_tag(q, "identity");
    xmlnode_put_attrib(info, "category", "client");
    xmlnode_put_attrib(info, "type", "pc");
    xmlnode_put_attrib(info, "name", user->nick);
}

void mt_con_browse_room_walk(xht h, const char *key, void *val, void *arg)
{
    sbr_user user = (sbr_user) val;
    xmlnode q = (xmlnode) arg;
    xmlnode x = xmlnode_insert_tag(q,"user");

    xmlnode_put_attrib(x,"jid",jid_full(user->lid));
    xmlnode_put_attrib(x,"name",user->nick);
}

void mt_con_disco_room_walk(xht h, const char *key, void *val, void *arg)
{
    sbr_user user = (sbr_user) val;
    xmlnode q = (xmlnode) arg;
    xmlnode x = xmlnode_insert_tag(q, "item");

    xmlnode_put_attrib(x, "jid", jid_full(user->lid));
    xmlnode_put_attrib(x, "name", user->nick);
}

void mt_con_browse_room(sbroom r, jpacket jp)
{
    xmlnode q, x;

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x,"conference");
    xmlnode_put_attrib(q,"xmlns",NS_BROWSE);
    xmlnode_put_attrib(q,"name",jp->to->user);
    xmlnode_put_attrib(q,"type","private");

    xhash_walk(r->users_mid,&mt_con_browse_room_walk,(void *) q);

    x = xmlnode_insert_tag(q,"user");
    xmlnode_put_attrib(x,"jid",r->uid);
    xmlnode_put_attrib(x,"name",r->nick);
}

void mt_con_disco_items_room(sbroom r, jpacket jp)
{
    xmlnode q;

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_ITEMS);

    xhash_walk(r->users_mid, &mt_con_disco_room_walk, (void *)q);
}

void mt_con_disco_info_room(sbroom r, jpacket jp)
{
    xmlnode q, info;

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_INFO);

    info = xmlnode_insert_tag(q, "identity");
    xmlnode_put_attrib(info, "category", "conference");
    xmlnode_put_attrib(info, "type", "text");
    xmlnode_put_attrib(info, "name", jp->to->user);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_CONFERENCE);
}

void mt_con_browse(session s, jpacket jp)
{
    if (jpacket_subtype(jp) == JPACKET__GET)
    {
        sbroom r = (sbroom) xhash_get(s->rooms,jp->to->user);
        if (r != NULL)
        {
            if (jp->to->resource == NULL)
                mt_con_browse_room(r,jp);
            else
                mt_con_browse_user(r,jp);
        }
        else
            jutil_error(jp->x,TERROR_NOTFOUND);
    }
    else
        jutil_error(jp->x,TERROR_NOTALLOWED);

    mt_deliver(s->ti,jp->x);
}

void mt_con_disco_items(session s, jpacket jp)
{
    if (jpacket_subtype(jp) != JPACKET__GET)
	jutil_error(jp->x, TERROR_NOTALLOWED);
    else
    {
	sbroom r = (sbroom) xhash_get(s->rooms, jp->to->user);
	if (r != NULL)
	    if (jp->to->resource == NULL)
		mt_con_disco_items_room(r, jp);
	    else
		mt_con_disco_items_user(r, jp);
	else
	    jutil_error(jp->x, TERROR_NOTFOUND);
    }

    mt_deliver(s->ti, jp->x);
}

void mt_con_disco_info(session s, jpacket jp)
{
    if (jpacket_subtype(jp) != JPACKET__GET)
	jutil_error(jp->x, TERROR_NOTALLOWED);
    else
    {
	sbroom r = (sbroom) xhash_get(s->rooms, jp->to->user);
	if (r != NULL)
	    if (jp->to->resource == NULL)
		mt_con_disco_info_room(r, jp);
	    else
		mt_con_disco_info_user(r, jp);
	else
	    jutil_error(jp->x, TERROR_NOTFOUND);
    }

    mt_deliver(s->ti, jp->x);
}

void mt_con_get(session s, jpacket jp)
{
    sbroom r;

    r = (sbroom) xhash_get(s->rooms,jp->to->user);
    if (r != NULL)
    {    
        xmlnode q;

        jutil_iqresult(jp->x);
        q = xmlnode_insert_tag(jp->x,"query");
        xmlnode_put_attrib(q,"xmlns",NS_CONFERENCE);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"name"),jp->to->user,-1);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"nick"),s->nick,-1);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"id"),r->uid,-1);
        deliver(dpacket_new(jp->x), NULL);
    }
    else
        jutil_error(jp->x,TERROR_NOTFOUND);

    mt_deliver(s->ti,jp->x);
}

void mt_con_set(session s, jpacket jp)
{
    xmlnode q;
    sbroom r;

    if (s->invites != NULL)
    {
        sbchat sc = (sbchat) xhash_get(s->invites,jp->to->user);
        if (sc != NULL)
        {
            mt_con_switch_mode(sc,jp,0);
            return;
        }
    }

    r = (sbroom) xhash_get(s->rooms,jp->to->user);
    if (r == NULL)
    {
        jid uid;
        char buf[20];

        r = mt_con_create(s,jp->to,xmlnode_get_tag_data(jp->iq,"name"),xmlnode_get_tag_data(jp->iq,"nick"));

        r->legacy = 0;
        snprintf(buf,20,"%X",r);
        uid = jid_new(jp->p,jid_full(jp->to));
        jid_set(uid,buf,JID_RESOURCE);
        r->uid = pstrdup(r->p,jid_full(uid));

        jutil_tofrom(jp->x);
        xmlnode_put_attrib(jp->x,"type","result");
        xmlnode_hide(xmlnode_get_tag(jp->iq,"id"));
        xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq,"id"),r->uid,-1);
    }
    else
    {
        jutil_iqresult(jp->x);
        q = xmlnode_insert_tag(jp->x,"query");
        xmlnode_put_attrib(q,"xmlns",NS_CONFERENCE);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"id"),r->uid,-1);
    }

    mt_deliver(s->ti,jp->x);
}

void mt_con_set_flush(jpacket jp, void *arg)
{
    session s = (session) arg;
    mt_con_set(s,jp);
}

void mt_con_iq_conference(session s, jpacket jp)
{
    if (jp->to->resource == NULL)
    {
        switch (jpacket_subtype(jp))
        {
        case JPACKET__SET:
            if (s->connected == 0)
                mt_jpbuf_en(s->buff,jp,&mt_con_set_flush,(void *) s);
            else
            mt_con_set(s,jp);
            break;

        case JPACKET__GET:
            mt_con_get(s,jp);
            break;

        default:
            jutil_error(jp->x,TERROR_BAD);
            mt_deliver(s->ti,jp->x);
            break;
        }
    }
    else
    {
        jutil_error(jp->x,TERROR_NOTALLOWED);
        mt_deliver(s->ti,jp->x);
    }
}

void mt_con_iq(session s, jpacket jp)
{
    char *xmlns = jp->iqns;

    if (jp->to->user != NULL)
    {
        if (j_strcmp(xmlns,NS_CONFERENCE) == 0)
            mt_con_iq_conference(s,jp);
        else if (j_strcmp(xmlns,NS_BROWSE) == 0)
            mt_con_browse(s,jp);
	else if (j_strcmp(xmlns,NS_DISCO_ITEMS) == 0)
	    mt_con_disco_items(s,jp);
	else if (j_strcmp(xmlns,NS_DISCO_INFO) == 0)
	    mt_con_disco_info(s,jp);
        else
            xmlnode_free(jp->x);
    }
    else
    {
        if (j_strcmp(xmlns,NS_BROWSE) == 0)
            mt_con_browse_server(s,jp);
	else if (j_strcmp(xmlns, NS_DISCO_ITEMS) == 0)
	    mt_con_disco_items_server(s, jp);
	else if (j_strcmp(xmlns, NS_DISCO_INFO) == 0)
	    mt_con_disco_info_server(s, jp);
        else
            mt_iq_server(s->ti,jp);
    }
}

void mt_con_presence_go(session s, jpacket jp)
{
    sbroom r;

    if (s->invites != NULL)
    {
        sbchat sc = (sbchat) xhash_get(s->invites,jp->to->user);
        if (sc != NULL)
        {
            mt_con_switch_mode(sc,jp,1);
            return;
        }
    }
    r = (sbroom) xhash_get(s->rooms,jp->to->user);

    if (r == NULL)
    {
        r = mt_con_create(s,jid_user(jp->to),jp->to->user,jp->to->resource);
        r->legacy = 1;
        r->uid = pstrdup(r->p,jid_full(jp->to));

        xmlnode_free(jp->x);
    }
    else
    {
        jutil_tofrom(jp->x);
        mt_deliver(s->ti,jp->x);
    }
}

void mt_con_presence_flush(jpacket jp, void *arg)
{
    session s = (session) arg;
    mt_con_presence_go(s,jp);
}

void mt_con_presence(session s, jpacket jp)
{
    sbroom r;

    if (jp->to->user == NULL)
    {
        xmlnode_free(jp->x);
        return;
    }

    switch (jpacket_subtype(jp))
    {
    case JPACKET__AVAILABLE:
        if (jp->to->resource != NULL)
        {
            if (s->connected == 0)
                mt_jpbuf_en(s->buff,jp,&mt_con_presence_flush,(void *) s);
            else
                mt_con_presence_go(s,jp);
        }
        else
            xmlnode_free(jp->x);
        break;

    case JPACKET__UNAVAILABLE:
        r = (sbroom) xhash_get(s->rooms,jp->to->user);
        if (r != NULL)
        {
            mt_con_end(r);
            xmlnode_free(jp->x);
        }
        else
        {
            jutil_tofrom(jp->x);
            mt_deliver(s->ti,jp->x);
        }
        break;

    default:
        xmlnode_free(jp->x);
        break;
    }
}

void mt_con_handle(session s, jpacket jp)
{
    switch (jp->type)
    {
    case JPACKET_PRESENCE:
        mt_con_presence(s,jp);
        break;

    case JPACKET_MESSAGE:
        mt_con_message(s,jp);
        break;

    case JPACKET_IQ:
        mt_con_iq(s,jp);
        break;
    }
}
