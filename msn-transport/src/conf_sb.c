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

result mt_con_closed(mpacket mp, void *arg);

void mt_con_free_walk(xht h, const char *key, void *val, void *arg)
{
    sbr_user user = (sbr_user) val;
    pool_free(user->p);
}

void mt_con_free(sbroom r)
{
    session s = r->s;
    xmlnode x;

    log_debug(ZONE,"freeing SB conference %X",r);

    if (r->legacy == 0)
    {
        xmlnode user;

        x = xmlnode_new_tag("iq");
        xmlnode_put_attrib(x,"to",xmlnode_get_attrib(ppdb_primary(s->p_db,s->id),"from"));
        xmlnode_put_attrib(x,"from",jid_full(r->rid));

        user = xmlnode_insert_tag(x,"user");
        xmlnode_put_attrib(user,"xmlns",NS_BROWSE);
        xmlnode_put_attrib(user,"jid",r->uid);
        xmlnode_put_attrib(user,"type","remove");

        mt_deliver(s->ti,x);
    }
    else
    {
        /* send unvail from room */
        x = jutil_presnew(JPACKET__UNAVAILABLE,jid_full(s->id),NULL);
        xmlnode_put_attrib(x,"from",r->uid);
        mt_deliver(s->ti,x);
    }

    xhash_walk(r->users_mid,&mt_con_free_walk,NULL);
    xhash_free(r->users_mid);
    xhash_free(r->users_lid);
    pool_free(r->p);
    SREF_DEC(s);
}

void mt_con_remove(sbroom r)
{
    log_debug(ZONE,"removing SB conference %X",r);

    assert(r->state != sb_CLOSE);
    r->state = sb_CLOSE;

    xhash_zap(r->s->rooms,r->rid->user);
}

void mt_con_end(sbroom r)
{
    mt_con_remove(r);

    if (r->st != NULL)
    {
        r->st->cb = &mt_con_closed;
        mt_stream_close(r->st);
    }
}

void mt_con_msg_send(sbroom r, char *mid, char *body)
{
    session s = r->s;
    xmlnode msg;
    sbr_user user;

    user = (sbr_user) xhash_get(r->users_mid,mid);

    msg = xmlnode_new_tag("message");
    xmlnode_put_attrib(msg,"type","groupchat");
    xmlnode_put_attrib(msg,"to",jid_full(s->id));
    xmlnode_put_attrib(msg,"from",jid_full(user->lid));

    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),body,-1);

    mt_deliver(s->ti,msg);
}

void mt_con_msg(sbroom r, mpacket mp)
{
    char *body, *ctype;

    ctype = strchr(mt_packet_data(mp,5),':') + 2;
    body = mt_packet_data(mp,mp->count - 1);

    if (j_strcmp(ctype,"text/plain; charset=UTF-8") != 0)
    {
        log_debug(ZONE,"Unknown content-type %s",ctype);
    }
    else
        mt_con_msg_send(r,mt_packet_data(mp,1),body);
}

void mt_con_bye(sbroom r, mpacket mp)
{
    session s = r->s;
    mti ti = s->ti;
    xmlnode x;
    sbr_user user;
    char *rid = jid_full(r->rid);

    --r->count;
    user = (sbr_user) xhash_get(r->users_mid,mt_packet_data(mp,1));
    xhash_zap(r->users_mid,user->mid);
    xhash_zap(r->users_lid,user->lid->resource);

    if (r->legacy)
    {
        x = xmlnode_new_tag("presence");
        xmlnode_put_attrib(x,"type","unavailable");
        xmlnode_put_attrib(x,"to",jid_full(s->id));
        xmlnode_put_attrib(x,"from",jid_full(user->lid));
    }
    else
    {
        xmlnode q;

        x = xmlnode_new_tag("iq");
        xmlnode_put_attrib(x,"type","set");
        xmlnode_put_attrib(x,"to",xmlnode_get_attrib(ppdb_primary(s->p_db,s->id),"from"));
        xmlnode_put_attrib(x,"from",rid);

        q = xmlnode_insert_tag(x, "user");
        xmlnode_put_attrib(q,"xmlns",NS_BROWSE);
        xmlnode_put_attrib(q,"jid",jid_full(user->lid));
        xmlnode_put_attrib(q,"type","remove");
    }

    mt_deliver(ti,x);

    x = xmlnode_new_tag("message");
    xmlnode_put_attrib(x,"to",jid_full(s->id));
    xmlnode_put_attrib(x,"from",rid);
    xmlnode_put_attrib(x,"type","groupchat");
    xmlnode_insert_cdata(xmlnode_insert_tag(x,"body"),spools(x->p,user->nick,ti->leave,x->p),-1);

    mt_deliver(ti,x);
    pool_free(user->p);
}

sbr_user mt_con_add(sbroom r, char *mid, char *nick)
{
    sbr_user user;
    pool p;
    char *lid;

    assert(mid && nick);

    p = pool_new();
    nick = mt_decode(p,nick);
    user = pmalloc(p,sizeof(_sbr_user));
    user->p = p;
    user->mid = pstrdup(p,mid);
    user->nick = pstrdup(p,nick);
    user->lid = jid_new(p,jid_full(r->rid));

    if (r->legacy == 0)
    {
        char buf[3];
        snprintf(buf,3,"%d",r->count);
        jid_set(user->lid,buf,JID_RESOURCE);
    }
    else
        jid_set(user->lid,nick,JID_RESOURCE);

    ++r->count;

    xhash_put(r->users_mid,user->mid,(void *) user);
    xhash_put(r->users_lid,user->lid->resource,(void *) user);

    return user;
}

void mt_con_joi(sbroom r, mpacket mp)
{
    session s = r->s;
    mti ti = s->ti;
    xmlnode x;
    sbr_user user;
    char *rid = jid_full(r->rid);

    user = mt_con_add(r,mt_packet_data(mp,1),mt_packet_data(mp,2));

    if (r->legacy)
    {
        x = xmlnode_new_tag("presence");
        xmlnode_put_attrib(x,"to",jid_full(s->id));
        xmlnode_put_attrib(x,"from",jid_full(user->lid));
    }
    else
    {
        xmlnode q;

        x = xmlnode_new_tag("iq");
        xmlnode_put_attrib(x,"type","set");
        xmlnode_put_attrib(x,"to",xmlnode_get_attrib(ppdb_primary(s->p_db,s->id),"from"));
        xmlnode_put_attrib(x,"from",jid_full(r->rid));

        q = xmlnode_insert_tag(x, "user");
        xmlnode_put_attrib(q,"xmlns",NS_BROWSE);
        xmlnode_put_attrib(q,"jid",jid_full(user->lid));
        xmlnode_put_attrib(q,"name",user->nick);
    }

    mt_deliver(ti,x);

    x = xmlnode_new_tag("message");
    xmlnode_put_attrib(x,"to",jid_full(s->id));
    xmlnode_put_attrib(x,"from",rid);
    xmlnode_put_attrib(x,"type","groupchat");
    xmlnode_insert_cdata(xmlnode_insert_tag(x,"body"),spools(x->p,user->nick,ti->join,x->p),-1);

    mt_deliver(ti,x);
}

void mt_con_ready(sbroom r)
{
    session s = r->s;
    mti ti = s->ti;
    xmlnode x;

    /* inform the client that the room is ready, and they joined it */
    if (r->legacy == 0)
    {
        xmlnode q;

        x = xmlnode_new_tag("iq");
        xmlnode_put_attrib(x,"type","set");
        xmlnode_put_attrib(x,"to",xmlnode_get_attrib(ppdb_primary(s->p_db,s->id),"from"));
        xmlnode_put_attrib(x,"from",jid_full(r->rid));

        q = xmlnode_insert_tag(x, "user");
        xmlnode_put_attrib(q,"xmlns",NS_BROWSE);
        xmlnode_put_attrib(q,"jid",r->uid);
        xmlnode_put_attrib(q,"name",r->nick);
    }
    else
    {
        x = jutil_presnew(JPACKET__AVAILABLE,jid_full(s->id),NULL);
        xmlnode_put_attrib(x,"from",r->uid);
    }

    mt_deliver(ti,x);

    x = xmlnode_new_tag("message");
    xmlnode_put_attrib(x,"to",jid_full(s->id));
    xmlnode_put_attrib(x,"from",jid_full(r->rid));
    xmlnode_put_attrib(x,"type","groupchat");
    xmlnode_insert_cdata(xmlnode_insert_tag(x,"body"),spools(x->p,r->nick,ti->join,x->p),-1);

    mt_deliver(ti,x);

    r->state = sb_READY;
}

result mt_con_closed(mpacket mp, void *arg)
{
    if (mp == NULL)
        mt_con_free((sbroom) arg);
    else if (j_strcmp(mt_packet_data(mp,0),"MSG") == 0)
        mt_con_msg((sbroom) arg,mp);

    return r_DONE;
}

result mt_con_packets(mpacket mp, void *arg)
{
    sbroom r = (sbroom) arg;

    if (mp != NULL)
    {
        char *cmd;

        cmd = mt_packet_data(mp,0);

        if (j_strcmp(cmd,"MSG") == 0)
            mt_con_msg(r,mp);
        else if (j_strcmp(cmd,"JOI") == 0)
            mt_con_joi(r,mp);
        else if (j_strcmp(cmd,"BYE") == 0)
            mt_con_bye(r,mp);
        else if (j_atoi(cmd,0) != 0)
            mt_con_end(r);
        else
            return r_ERR;
    }
    else
    {
        mt_con_remove(r);
        mt_con_free(r);
    }

    return r_DONE;
}

result mt_con_usr(mpacket mp, void *arg)
{
    sbroom r = (sbroom) arg;

    if (j_strcmp(mt_packet_data(mp,0),"USR") == 0)
    {
        mt_con_ready(r);
    }
    else if (j_atoi(mt_packet_data(mp,0),0))
        mt_con_end(r);
    else
        return r_ERR;

    return r_DONE;
}

result mt_con_xfr(mpacket mp, void *arg)
{
    sbroom r = (sbroom) arg;

    if (r->state == sb_CLOSE)
    {
        mt_con_free(r);
        return r_DONE;
    }

    if (j_strcmp(mt_packet_data(mp,0),"XFR") == 0 && j_strcmp(mt_packet_data(mp,2),"SB") == 0)
    {
        mpstream st;
        char *host, *port;

        host = mt_packet_data(mp,3);
        port = strchr(host,':');
        if (port != NULL)
        {
            *port = '\0';
            ++port;
        }

        st = r->st = mt_stream_connect(host,j_atoi(port,1863),&mt_con_packets,(void *) r);
        mt_stream_register(st,&mt_con_usr,(void *) r);
        mt_cmd_usr(st,r->s->user,mt_packet_data(mp,5));        
    }
    else
    {
        mt_con_remove(r);
        mt_con_free(r);
    }

    return r_DONE;
}

void mt_con_switch_mode(sbchat sc, jpacket jp, int legacy)
{
    sbroom r;
    pool p;
    sbc_user cur;
    xmlnode x;
    session s = sc->s;
    mti ti = s->ti;
    sbr_user user;

    assert(sc->state == sb_READY && sc->st != NULL);

    /* the chat mode already up'd the ref count */

    p = pool_new();
    r = pmalloc(p,sizeof(_sbroom));
    r->p = p;
    r->s = s;
    r->st = sc->st;
    r->state = sb_READY;
    r->legacy = legacy;
    r->rid = jid_new(p,s->ti->con_id);
    jid_set(r->rid,sc->invite_id,JID_USER);

    if (legacy)
    {
        r->name = pstrdup(p,sc->invite_id);
        r->nick = pstrdup(p,jp->to->resource);
    }
    else
    {
        r->name = pstrdup(p,xmlnode_get_tag_data(jp->iq,"name"));
        if (r->name == NULL)
            r->name = pstrdup(p,sc->invite_id);

        r->nick = xmlnode_get_tag_data(jp->iq,"nick");
    }

    r->users_mid = xhash_new(5);
    r->users_lid = xhash_new(5);
    r->count = sc->count;

    r->st->cb = &mt_con_packets;
    r->st->arg = (void *) r;

    xhash_put(s->rooms,r->rid->user,(void *) r);

    if (legacy)
    {
        r->uid = pstrdup(r->p,jid_full(jp->to));
        jutil_tofrom(jp->x);
    }
    else
    {
        jid uid;
        char buf[20];

        snprintf(buf,20,"%X",r);
        uid = jid_new(jp->p,jid_full(jp->to));
        jid_set(uid,buf,JID_RESOURCE);
        r->uid = pstrdup(r->p,jid_full(uid));

        jutil_tofrom(jp->x);
        xmlnode_put_attrib(jp->x,"type","result");
        xmlnode_hide(xmlnode_get_tag(jp->iq,"id"));
        xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq,"id"),r->uid,-1);
    }

    mt_deliver(s->ti,jp->x);

    x = xmlnode_new_tag("message");
    xmlnode_put_attrib(x,"to",jid_full(s->id));
    xmlnode_put_attrib(x,"from",jid_full(r->rid));
    xmlnode_put_attrib(x,"type","groupchat");
    xmlnode_insert_cdata(xmlnode_insert_tag(x,"body"),spools(x->p,r->nick,ti->join,x->p),-1);

    mt_deliver(ti,x);

    for (cur = sc->users; cur != NULL; cur = cur->next)
    {
        char *tmp;

        tmp = pstrdup(sc->p,cur->mid);
        *(strchr(tmp,'@')) = '\0';
        user = mt_con_add(r,cur->mid,tmp); // FIXME, make sbc_chat->nick, and use that

        if (legacy)
        {
            x = xmlnode_new_tag("presence");
            xmlnode_put_attrib(x,"to",jid_full(s->id));
            xmlnode_put_attrib(x,"from",jid_full(user->lid));
        }
        else
        {
            xmlnode q;

            x = xmlnode_new_tag("iq");
            xmlnode_put_attrib(x,"type","set");
            xmlnode_put_attrib(x,"to",xmlnode_get_attrib(ppdb_primary(s->p_db,s->id),"from"));
            xmlnode_put_attrib(x,"from",jid_full(r->rid));

            q = xmlnode_insert_tag(x, "user");
            xmlnode_put_attrib(q,"xmlns",NS_BROWSE);
            xmlnode_put_attrib(q,"jid",jid_full(user->lid));
            xmlnode_put_attrib(q,"name",user->nick);
        }

        mt_deliver(ti,x);

        x = xmlnode_new_tag("message");
        xmlnode_put_attrib(x,"to",jid_full(s->id));
        xmlnode_put_attrib(x,"from",jid_full(r->rid));
        xmlnode_put_attrib(x,"type","groupchat");
        xmlnode_insert_cdata(xmlnode_insert_tag(x,"body"),spools(x->p,user->nick,ti->join,x->p),-1);

        mt_deliver(ti,x);
    }

    mt_chat_remove(sc);
    mt_free(sc->thread);
    pool_free(sc->p);
}

sbroom mt_con_create(session s, jid rid, char *name, char *nick)
{
    sbroom r;
    pool p;
    mpstream st = s->st;
    char *rjid;

    assert(rid->resource == NULL);
    SREF_INC(s);

    p = pool_new();
    r = pmalloc(p,sizeof(_sbroom));
    r->p = p;
    r->s = s;
    r->st = NULL;
    r->state = sb_START;
    rjid = jid_full(rid);
    lowercase(rjid);
    r->rid = jid_new(p,rjid);
    r->name = pstrdup(p,name);
    lowercase(r->name);
    r->nick = pstrdup(p,nick);
    r->users_mid = xhash_new(5);
    r->users_lid = xhash_new(5);
    r->count = 0;

    xhash_put(s->rooms,r->rid->user,(void *) r);

    mt_stream_register(st,&mt_con_xfr,(void *) r);
    mt_cmd_xfr_sb(st);

    return r;
}
