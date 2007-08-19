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
#include <sys/utsname.h>

void mt_iq_version(mti ti, jpacket jp)
{
    if (jpacket_subtype(jp) == JPACKET__GET)
    {
        xmlnode os, q;
        struct utsname un;

        q = xmlnode_insert_tag(jutil_iqresult(jp->x),"query");
        xmlnode_put_attrib(q,"xmlns",NS_VERSION);

        xmlnode_insert_cdata(xmlnode_insert_tag(q,"name"),"MSN Transport",-1);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"version"),VERSION,-1);

        uname(&un);
        os = xmlnode_insert_tag(q,"os");
        xmlnode_insert_cdata(os,un.sysname,-1);
        xmlnode_insert_cdata(os," ",1);
        xmlnode_insert_cdata(os,un.release,-1);
    }
    else
        jutil_error(jp->x,TERROR_BAD);

    mt_deliver(ti,jp->x);
}

void mt_iq_time(mti ti, jpacket jp)
{
    if (jpacket_subtype(jp) == JPACKET__GET)
    {
        xmlnode q;

        q = xmlnode_insert_tag(jutil_iqresult(jp->x),"query");
        xmlnode_put_attrib(q,"xmlns",NS_TIME);

        xmlnode_insert_cdata(xmlnode_insert_tag(q,"utc"),jutil_timestamp(),-1);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"tz"),tzname[0],-1);
    }
    else
        jutil_error(jp->x,TERROR_NOTALLOWED);

    mt_deliver(ti,jp->x);
}

void mt_iq_last(mti ti, jpacket jp)
{
    if (jpacket_subtype(jp) == JPACKET__GET)
    {
        xmlnode q;
        char str[10];

        q = xmlnode_insert_tag(jutil_iqresult(jp->x),"query");
        xmlnode_put_attrib(q,"xmlns",NS_LAST);

        snprintf(str,10,"%d",time(NULL) - ti->start);
        xmlnode_put_attrib(q,"seconds",str);
    }
    else
        jutil_error(jp->x,TERROR_NOTALLOWED);

    mt_deliver(ti,jp->x);
}

void mt_iq_gateway(mti ti, jpacket jp)
{
    if (jpacket_subtype(jp) == JPACKET__SET)
    {
        char *user, *ptr;

        user = xmlnode_get_tag_data(jp->iq,"prompt");

        if (user && (ptr = strchr(user,'@')))
        {
            xmlnode q;

            *ptr = '%';
            user = spools(jp->p,user,"@",jp->to->server,jp->p);

            q = xmlnode_insert_tag(jutil_iqresult(jp->x),"query");
            xmlnode_put_attrib(q,"xmlns",NS_GATEWAY);
            xmlnode_insert_cdata(xmlnode_insert_tag(q,"prompt"),user,-1);
        }
        else
            jutil_error(jp->x,TERROR_BAD);
    }
    else
    {
        if (jp->to->user == NULL)
        {
            xmlnode q;

            q = xmlnode_insert_tag(jutil_iqresult(jp->x),"query");
            xmlnode_put_attrib(q,"xmlns",NS_GATEWAY);
            xmlnode_insert_cdata(xmlnode_insert_tag(q,"desc"),"Enter the user's MSN account",-1);
            xmlnode_insert_tag(q,"prompt");
        }
        else
            jutil_error(jp->x,TERROR_NOTALLOWED);
    }

    mt_deliver(ti,jp->x);
}

void mt_iq_admin_who(xht h, const char *key, void *data, void *arg)
{
    session s = (session) data;
    xmlnode who = (xmlnode) arg;
    xmlnode x;

    x = xmlnode_insert_tag(who,"presence");
    xmlnode_put_attrib(x,"from",jid_full(s->id));
}

void mt_iq_admin(mti ti, jpacket jp)
{
    xmlnode x;

    if (jpacket_subtype(jp) == JPACKET__GET  && ti->admin != NULL &&
        xmlnode_get_tag(ti->admin,spools(jp->p,"read=",jid_full(jid_user(jp->from)),jp->p)) != NULL)
    {
        if((x = xmlnode_get_tag(jp->iq,"who")))
            xhash_walk(ti->sessions,mt_iq_admin_who,(void *) x);
        else if (xmlnode_get_tag(jp->iq,"pool"))
            pool_stat(1);

        jutil_tofrom(jp->x);
        xmlnode_put_attrib(jp->x,"type","result");
    }
    else
        jutil_error(jp->x,TERROR_NOTALLOWED);

    mt_deliver(ti,jp->x);
}

void mt_iq_vcard_server(mti ti, jpacket jp)
{
    if (jpacket_subtype(jp) == JPACKET__GET)
        xmlnode_insert_node(jutil_iqresult(jp->x),ti->vcard);
    else
        jutil_error(jp->x,TERROR_NOTALLOWED);

    mt_deliver(ti,jp->x);
}

void mt_iq_browse_server(mti ti, jpacket jp)
{
    if (jpacket_subtype(jp) == JPACKET__GET)
    {
        xmlnode q;

        q = xmlnode_insert_tag(jutil_iqresult(jp->x),"service");
        xmlnode_put_attrib(q,"xmlns",NS_BROWSE);

        xmlnode_put_attrib(q,"type","msn");
        xmlnode_put_attrib(q,"jid",jp->to->server);
        xmlnode_put_attrib(q,"name",xmlnode_get_tag_data(ti->vcard,"FN"));

        xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_REGISTER,-1);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_GATEWAY,-1);

        if (ti->con)
        {
            xmlnode c = xmlnode_insert_tag(q,"conference");
            xmlnode_put_attrib(c,"type","private");
            xmlnode_put_attrib(c,"jid",ti->con_id);
            xmlnode_put_attrib(c,"name","MSN Conference");
        }
    }
    else
        jutil_error(jp->x,TERROR_NOTALLOWED);

    mt_deliver(ti,jp->x);
}

void mt_iq_disco_items_server(mti ti, jpacket jp)
{
    xmlnode q;

    if (jpacket_subtype(jp) != JPACKET__GET)
    {
	jutil_error(jp->x,TERROR_NOTALLOWED);
	mt_deliver(ti, jp->x);
	return;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_ITEMS);

    if (ti->con)
    {
	xmlnode item = xmlnode_insert_tag(q,"item");
	xmlnode_put_attrib(item, "name", "MSN Conference");
	xmlnode_put_attrib(item, "jid", ti->con_id);
    }

    mt_deliver(ti,jp->x);
}

void mt_iq_disco_info_server(mti ti, jpacket jp)
{
    xmlnode q, info;

    if (jpacket_subtype(jp) != JPACKET__GET)
    {
	jutil_error(jp->x,TERROR_NOTALLOWED);
	mt_deliver(ti, jp->x);
	return;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_INFO);
    info = xmlnode_insert_tag(q, "identity");
    xmlnode_put_attrib(info, "category", "gateway");
    xmlnode_put_attrib(info, "type", "msn");
    xmlnode_put_attrib(info, "name",xmlnode_get_tag_data(ti->vcard,"FN"));

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_REGISTER);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_VERSION);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_TIME);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_LAST);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_GATEWAY);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_VCARD);

    if (ti->admin != NULL && xmlnode_get_tag(ti->admin,spools(jp->p,"read=",jid_full(jid_user(jp->from)),jp->p)) != NULL)
    {
	info = xmlnode_insert_tag(q, "feature");
	xmlnode_put_attrib(info, "var", NS_ADMIN);
    }

    mt_deliver(ti,jp->x);
}

void mt_iq_vcard_user(session s, jpacket jp)
{
    xmlnode q;
    muser u;
    char *m;

    if (jpacket_subtype(jp) == JPACKET__GET && (m = mt_jid2mid(jp->p,jp->to)) != NULL)
    {
        q = xmlnode_insert_tag(jutil_iqresult(jp->x),"vCard");
        xmlnode_put_attrib(q,"xmlns",NS_VCARD);

        u = (muser) xhash_get(s->users,m);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"NICKNAME"),
                             u != NULL ? mt_decode(jp->p,u->handle) : m,-1);
    }
    else
        jutil_error(jp->x,TERROR_BAD);

    mt_deliver(s->ti,jp->x);
}

void mt_iq_browse_user(session s, jpacket jp)
{
    xmlnode browse;
    muser u;
    char *m;

    if (jpacket_subtype(jp) == JPACKET__GET && (m = mt_jid2mid(jp->p,jp->to)) != NULL)
    {
        browse = xmlnode_insert_tag(jutil_iqresult(jp->x),"user");
        xmlnode_put_attrib(browse,"xmlns",NS_BROWSE);

        xmlnode_put_attrib(browse,"jid",jid_full(jid_user(jp->to)));
        xmlnode_put_attrib(browse,"type","user");

        u = (muser) xhash_get(s->users,m);
        xmlnode_put_attrib(browse,"name",u != NULL ? mt_decode(jp->p,u->handle) : m);
    }
    else
        jutil_error(jp->x,TERROR_BAD);

    mt_deliver(s->ti,jp->x);
}

void mt_iq_disco_items_user(session s, jpacket jp)
{
    xmlnode q;
    char *m;

    if (jpacket_subtype(jp) != JPACKET__GET || (m = mt_jid2mid(jp->p, jp->to)) == NULL)
	jutil_error(jp->x, TERROR_BAD);

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_ITEMS);

    mt_deliver(s->ti, jp->x);
}

void mt_iq_disco_info_user(session s, jpacket jp)
{
    xmlnode q, info;
    muser u;
    char *m = NULL;

    if (jpacket_subtype(jp) != JPACKET__GET || (m = mt_jid2mid(jp->p, jp->to)) == NULL)
	jutil_error(jp->x, TERROR_BAD);

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_INFO);

    info = xmlnode_insert_tag(q, "identity");
    xmlnode_put_attrib(info, "category", "client");
    xmlnode_put_attrib(info, "type", "pc");
    u = (muser) xhash_get(s->users,m);
    xmlnode_put_attrib(info,"name",u != NULL ? mt_decode(jp->p,u->handle) : m);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_VERSION);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_VCARD);

    mt_deliver(s->ti, jp->x);
}

typedef void (*iq_server_cb)(mti ti, jpacket jp);

void mt_iq_server(mti ti, jpacket jp)
{
    char *xmlns = jp->iqns;
    iq_server_cb cb;

    cb = (iq_server_cb) xhash_get(ti->iq_handlers,xmlns);
    if (cb == NULL)
    {
        jutil_error(jp->x,TERROR_NOTALLOWED);
        mt_deliver(ti,jp->x);
    }
    else
        (cb)(ti,jp);
}

void mt_iq_init(mti ti)
{
    xht h;

    h = ti->iq_handlers = xhash_new(5);

    xhash_put(h,NS_VERSION,&mt_iq_version);
    xhash_put(h,NS_TIME,&mt_iq_time);
    xhash_put(h,NS_LAST,&mt_iq_last);
    xhash_put(h,NS_GATEWAY,&mt_iq_gateway);
    xhash_put(h,NS_ADMIN,&mt_iq_admin);
    xhash_put(h,NS_VCARD,&mt_iq_vcard_server);
    xhash_put(h,NS_BROWSE,&mt_iq_browse_server);
    xhash_put(h,NS_DISCO_ITEMS,&mt_iq_disco_items_server);
    xhash_put(h,NS_DISCO_INFO,&mt_iq_disco_info_server);
}

void mt_iq(session s, jpacket jp)
{
    mti ti = s->ti;
    char *xmlns = jp->iqns;

    if (jp->to->user == NULL)
    {
        if (j_strcmp(xmlns,NS_REGISTER) == 0)
            mt_reg_session(s,jp);
        else
            mt_iq_server(ti,jp);
    }
    else
    {
        if (j_strcmp(xmlns,NS_VCARD) == 0)
            mt_iq_vcard_user(s,jp);
        else if (j_strcmp(xmlns,NS_BROWSE) == 0)
            mt_iq_browse_user(s,jp);
        else if (j_strcmp(xmlns,NS_VERSION) == 0)
            mt_iq_version(s->ti,jp);
	else if (j_strcmp(xmlns,NS_DISCO_ITEMS) == 0)
	    mt_iq_disco_items_user(s,jp);
	else if (j_strcmp(xmlns,NS_DISCO_INFO) == 0)
	    mt_iq_disco_info_user(s,jp);
        else
        {
            jutil_error(jp->x,TERROR_NOTALLOWED);
            mt_deliver(ti,jp->x);
        }
    }
}
