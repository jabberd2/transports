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


void mt_chat_lcomposing(sbchat sc);
void mt_chat_rcomposing(sbchat sc, int y);
result mt_chat_closed(mpacket mp, void *arg);

void mt_chat_docomposing_chat(xht h, const char *key, void *val, void *arg)
{
    sbchat sc = (sbchat) val;
    if(sc->comp) {
        if(sc->rcomp_counter >= 0)
            sc->rcomp_counter++;
        if(sc->rcomp_counter == 3)
            mt_chat_rcomposing(sc, 0);
        if(sc->lcomp_active == 1)
            mt_chat_lcomposing(sc);
    }
}

void mt_chat_docomposing_session(xht h, const char *key, void *val, void *arg)
{
    session s = (session) val;
    /* Walk through every sbchat */
    xhash_walk(s->chats,&mt_chat_docomposing_chat,NULL);
}

result mt_chat_docomposing(void *arg)
{
    mti ti = (mti) arg;
    /* Walk through every session */
    xhash_walk(ti->sessions,&mt_chat_docomposing_session,NULL);
    return r_DONE;
}

void mt_chat_free(sbchat sc)
{
    session s = sc->s;
    jpacket jp;

    log_debug(ZONE,"freeing SB chat %X",sc);

    mt_free(sc->thread);

    while ((jp = mt_jpbuf_de(sc->buff)) != NULL)
    {
        jutil_error(jp->x,TERROR_EXTERNAL);
        mt_deliver(s->ti,jp->x);
    }

    pool_free(sc->p);
    SREF_DEC(s);
}

void mt_chat_remove(sbchat sc)
{
    session s = sc->s;
    sbc_user cur;

    log_debug(ZONE,"removing SB chat %X",sc);

    assert(sc->state != sb_CLOSE);
    sc->state = sb_CLOSE;

    for (cur = sc->users; cur != NULL; cur = cur->next)
        xhash_zap(s->chats,cur->mid);

    if (sc->invite_id)
        xhash_zap(s->invites,sc->invite_id);
}

void mt_chat_end(sbchat sc)
{
    mt_chat_remove(sc);

    if (sc->st != NULL)
    {
        sc->st->cb = &mt_chat_closed;
        mt_stream_close(sc->st);
    }
}

sbc_user mt_chat_add(sbchat sc, char *mid)
{
    sbc_user su;
    pool p = sc->p;

    su = pmalloc(p,sizeof(_sbc_user));
    su->mid = pstrdup(p,mid);

    su->next = sc->users;
    sc->users = su;

    xhash_put(sc->s->chats,su->mid,(void *) sc);

    return su;
}

void mt_chat_bye(sbchat sc, mpacket mp)
{
    char *mid = mt_packet_data(mp,1);

    log_debug(ZONE,"User '%s' left, %d",mid,sc->count);

    if (--sc->count != 0)
    {
        sbc_user cur, prev;

        for (prev = NULL, cur = sc->users; cur != NULL; prev = cur, cur = cur->next)
        {
            if (j_strcmp(cur->mid,mid) == 0)
            {
                if (prev)
                    prev->next = cur->next;
                else
                    sc->users = cur->next;
                break;
            }
        }

        xhash_zap(sc->s->chats,mid);
    }
    else
        mt_chat_end(sc);
}

void mt_chat_invite(sbchat sc, char *user)
{
    session s = sc->s;
    xmlnode msg, x;
    char buf[40];

    snprintf(buf,40,"%X",sc);
    lowercase(buf);

    sc->invite_id = pstrdup(sc->p,buf);
    log_debug(ZONE,"SB invite %s",sc->invite_id);
    xhash_put(s->invites,sc->invite_id,(void *) sc);

    msg = xmlnode_new_tag("message");
    xmlnode_put_attrib(msg,"to",jid_full(s->id));
    xmlnode_put_attrib(msg,"from",mt_mid2jid_full(msg->p,user,s->host));
    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),s->ti->invite_msg,-1);
    x = xmlnode_insert_tag(msg,"x");
    xmlnode_put_attrib(x,"jid",spools(msg->p,sc->invite_id,"@",s->ti->con_id,msg->p));
    xmlnode_put_attrib(x,"xmlns","jabber:x:conference");

    mt_deliver(s->ti,msg);
}

void mt_chat_joied(sbchat sc, char *user)
{
    sbchat old;

    ++sc->count;
    if ((old = xhash_get(sc->s->chats,user)) != NULL)
    {
        if (old != sc)
        {
            mt_chat_end(old);  /* leave the other session */
            mt_chat_add(sc,user);
        }
    }
    else
        mt_chat_add(sc,user);

    if (sc->count == 2 && !sc->invite_id && sc->s->invites)
        mt_chat_invite(sc,user);
}

void mt_chat_joi(sbchat sc, mpacket mp)
{
    mt_chat_joied(sc,mt_packet_data(mp,1));

    if (sc->state != sb_READY)
    { /* make sure the other users joins before we start send messages */
        sc->state = sb_READY;
        mt_jpbuf_flush(sc->buff);
    }
}

void mt_chat_text(sbchat sc, mpacket mp)
{
    session s = sc->s;
    xmlnode x, msg;
    char *user = mt_mid2jid_full(mp->p,mt_packet_data(mp,1),s->host);
    char *body = mt_packet_data(mp,mp->count - 1);
    char *fmt = mt_packet_data(mp,mp->count - 2);

    if (strncmp(fmt,"X-MMS-IM-Format",15))
    {
        log_debug(ZONE,"Unknown format '%s'",fmt);
        fmt = NULL;
    }

    msg = xmlnode_new_tag("message");
    xmlnode_put_attrib(msg,"to",jid_full(s->id));
    xmlnode_put_attrib(msg,"from",user);
    xmlnode_put_attrib(msg,"type","chat");

    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"thread"),sc->thread,-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),body,-1);

    if (sc->xhtml == 1 && fmt != NULL)
        mt_xhtml_message(msg,fmt,body);

    x = xmlnode_insert_tag(msg,"x");
    xmlnode_put_attrib(x,"xmlns","jabber:x:event");
    xmlnode_insert_tag(x,"composing");

    sc->rcomp_counter = -1;
    mt_deliver(s->ti,msg);
}

void mt_chat_comp(sbchat sc, mpacket mp)
{
    if (sc->comp)
    {
        if(sc->rcomp_counter == -1) {
            mt_chat_rcomposing(sc, 1);
        }
        else if(sc->rcomp_counter > 0) {
            sc->rcomp_counter = 0;
        }
    }
}

void mt_chat_lcomposing(sbchat sc)
{
    if(sc->comp) {
        char buf[512];
        snprintf(buf,512,MIME_TYPE,sc->s->user);
        mt_cmd_msg(sc->st,"U",buf);
    }
}

void mt_chat_rcomposing(sbchat sc, int y)
{
    session s = sc->s;
    xmlnode x, msg;
    char *user = NULL;

    if (!sc->comp) return;
    
    user = mt_mid2jid_full(sc->p,sc->users->mid,s->host);
    msg = xmlnode_new_tag("message");
    xmlnode_put_attrib(msg,"to",jid_full(s->id));
    xmlnode_put_attrib(msg,"from",user);
    xmlnode_put_attrib(msg,"type","chat");

    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"thread"),sc->thread,-1);

    x = xmlnode_insert_tag(msg,"x");
    xmlnode_put_attrib(x,"xmlns","jabber:x:event");
    if(y) {
        xmlnode_insert_tag(x,"composing");
        sc->rcomp_counter = 0;
    }
    else {
        sc->rcomp_counter = -1;
    }
    xmlnode_insert_cdata(xmlnode_insert_tag(x,"id"),sc->thread,-1);
    mt_deliver(s->ti,msg);
}

void mt_chat_msg(sbchat sc, mpacket mp)
{
    char *ctype;

    /* I've seen some MSGs sent without a MIME header... */
    if (strncmp(mt_packet_data(mp,5),"Content-Type: ",14) == 0)
        ctype = mt_packet_data(mp,5);
    else if (strncmp(mt_packet_data(mp,4),"Content-Type: ",14) == 0)
        ctype = mt_packet_data(mp,4);
    else
    {
        log_debug(ZONE,"Invalid message sent from '%s: couldn't find Content-Type",mt_packet_data(mp,1));
        return;
    }

    ctype += 14;

    if (j_strcmp(ctype,"text/x-msmsgscontrol") == 0)
    {
        mt_chat_comp(sc,mp);
    }
    else if (j_strcmp(ctype,"text/plain; charset=UTF-8") == 0)
    {
        mt_chat_text(sc,mp);
    }
    else
    {
        log_debug(ZONE,"Unknown content-type: %s",ctype);
    }
}

result mt_chat_closed(mpacket mp, void *arg)
{
    if (mp == NULL)
        mt_chat_free((sbchat) arg);
    else if (j_strcmp(mt_packet_data(mp,0),"MSG") == 0)
        mt_chat_msg((sbchat) arg,mp);

    return r_DONE;
}

result mt_chat_packets(mpacket mp, void *arg)
{
    sbchat sc = (sbchat) arg;

    if (mp != NULL)
    {
        char *cmd;

        cmd = mt_packet_data(mp,0);

        if (j_strcmp(cmd,"MSG") == 0)
            mt_chat_msg(sc,mp);
        else if (j_strcmp(cmd,"JOI") == 0)
            mt_chat_joi(sc,mp);
        else if (j_strcmp(cmd,"BYE") == 0)
            mt_chat_bye(sc,mp);
        else if (j_atoi(cmd,0) != 0)
            mt_chat_end(sc);
        else
            return r_ERR;
    }
    else
    {
        mt_chat_remove(sc);
        mt_chat_free(sc);
    }

    return r_DONE;
}

void mt_chat_write(sbchat sc, jpacket jp)
{
    xmlnode cur;
    char *body = NULL, *name;

    for_each_node(cur,jp->x)
    {
        if (xmlnode_get_type(cur) != NTYPE_TAG) continue;
        name = xmlnode_get_name(cur);

        if (strcmp(name,"thread") == 0)
        {
            char *thread = xmlnode_get_data(cur);
            if (thread != NULL)
            {
                mt_free(sc->thread);
                sc->thread = mt_strdup(thread);
            }
        }
        else if (strcmp(name,"body") == 0)
        {
            char *data;
            if (body == NULL && (data = xmlnode_get_data(cur)) != NULL)
            {
                spool sp = spool_new(jp->p);
                spool_add(sp,MIME_MSG);
                mt_replace_newline(sp,data);
                body = spool_print(sp);
            }
        }
        else
        {
            char *xmlns = xmlnode_get_attrib(cur,"xmlns");
            if (xmlns != NULL)
            {
                if (strcmp(xmlns,NS_XHTML) == 0)
                {
                    char *tmp = mt_xhtml_format(cur);
                    if (tmp != NULL)
                        body = tmp;
                }
                else if (strcmp(xmlns,"jabber:x:event") == 0) {
                    /* If there's a composing tag then they are composing, so set the lcomp flag to active */
                    sc->comp = 1;
                    if(xmlnode_get_tag(cur, "composing") != NULL) {
                        sc->lcomp_active = 1;
                        mt_chat_lcomposing(sc);
                        log_debug(ZONE, "lcomp_active = 1");
                    }
                    else {
                        sc->lcomp_active = 0;
                        log_debug(ZONE, "lcomp_active = 0");
                    }
                }
            }
        }
    }

    if(body != NULL) {
        mt_cmd_msg(sc->st,"U",body);
        sc->lcomp_active = 0;
        log_debug(ZONE, "lcomp_active = 0");
    }

    xmlnode_free(jp->x);
}

result mt_chat_cal(mpacket mp, void *arg)
{
    if (j_strcmp(mt_packet_data(mp,0),"CAL"))
    {
        sbchat sc = (sbchat) arg;

        if (j_atoi(mt_packet_data(mp,0),0) == 217)
        {
            jpacket jp;

            while ((jp = mt_jpbuf_de(sc->buff)) != NULL)
            {
                jutil_error(jp->x,(terror){405,"User is offline"});
                mt_deliver(sc->s->ti,jp->x);
            }
        }
        mt_chat_end(sc);
    }

    return r_DONE;
}

result mt_chat_usr(mpacket mp, void *arg)
{
    sbchat sc = (sbchat) arg;

    if (j_strcmp(mt_packet_data(mp,0),"USR") == 0)
    { /* send the initial invite */
        mt_stream_register(sc->st,&mt_chat_cal,(void *) sc);
        mt_cmd_cal(sc->st,sc->users->mid);
    }
    else if (j_atoi(mt_packet_data(mp,0),0))
        mt_chat_end(sc);
    else
        return r_ERR;

    return r_DONE;
}

result mt_chat_xfr(mpacket mp, void *arg)
{
    sbchat sc = (sbchat) arg;

    if (sc->state == sb_CLOSE)
    {
        mt_chat_free(sc);
        return r_DONE;
    }

    if (j_strcmp(mt_packet_data(mp,0),"XFR") == 0 && j_strcmp(mt_packet_data(mp,2),"SB") == 0)
    {
        session s = sc->s;
        mpstream st;
        char *host, *port;

        host = mt_packet_data(mp,3);
        port = strchr(host,':');
        if (port != NULL)
        {
            *port = '\0';
            ++port;
        }

        st = sc->st = mt_stream_connect(host,j_atoi(port,1863),&mt_chat_packets,(void *) sc);
        mt_stream_register(st,&mt_chat_usr,(void *) sc);
        mt_cmd_usr(st,s->user,mt_packet_data(mp,5));
    }
    else
    {
        mt_chat_remove(sc);
        mt_chat_free(sc);
    }

    return r_DONE;
}

result mt_chat_ans(mpacket mp, void *arg)
{
    sbchat sc = (sbchat) arg;
    char *cmd;

    cmd = mt_packet_data(mp,0);
    if (j_strcmp(cmd,"IRO") == 0)
    {
        mt_chat_joied(sc,mt_packet_data(mp,4));
        return r_PASS;
    }

    if (j_strcmp(cmd,"ANS") == 0 && sc->users != NULL)
    {
        sc->state = sb_READY;
        mt_jpbuf_flush(sc->buff);
    }
    else
    {
        log_debug(ZONE,"SB session, ANS error, %s",cmd);
        mt_chat_end(sc);
    }

    return r_DONE;
}

sbchat mt_chat_new(session s, char *user)
{
    sbchat sc;
    pool p;

    SREF_INC(s);

    p = pool_new();
    sc = pmalloc(p,sizeof(_sbchat));
    sc->p = p;
    sc->s = s;
    sc->st = NULL;
    sc->state = sb_START;
    sc->buff = mt_jpbuf_new(p);
    sc->thread = mt_strdup(shahash(user)); /* make a default thread */
    sc->invite_id = NULL;
    sc->comp = 0;
    sc->lcomp_active = 0;
    sc->rcomp_counter = -1;
    sc->xhtml = 1;
    sc->count = sc->count = 0;
    sc->users = NULL;

    mt_chat_add(sc,user);

    return sc;
}

void mt_chat_join(session s, char *user, char *host, int port, char *chal, char *sid)
{
    sbchat sc;
    mpstream st;

    sc = mt_chat_new(s,user);
    st = sc->st = mt_stream_connect(host,port,&mt_chat_packets,(void *) sc);
    mt_stream_register(st,&mt_chat_ans,(void *) sc);
    mt_cmd_ans(st,sc->s->user,chal,sid);
}

void mt_chat_send_flush(jpacket jp, void *arg)
{
    sbchat sc = (sbchat) arg;
    mt_chat_write(sc,jp);
}

void mt_chat_send(session s, jpacket jp, char *to)
{
    sbchat sc;

    sc = (sbchat) xhash_get(s->chats,to);
    if (sc == NULL)
    {
        sc = mt_chat_new(s,to);
        mt_stream_register(s->st,&mt_chat_xfr,(void *) sc);
        mt_cmd_xfr_sb(s->st);
        mt_jpbuf_en(sc->buff,jp,&mt_chat_send_flush,(void *) sc);
    }
    else if (sc->state == sb_READY)
        mt_chat_write(sc,jp);
    else
        mt_jpbuf_en(sc->buff,jp,&mt_chat_send_flush,(void *) sc);
}

void mt_chat_message_flush(jpacket jp, void *arg)
{
    session s = (session) arg;
    char *user = (char *) jp->aux1;
    mt_chat_send(s,jp,user);
}

void mt_chat_message(session s, jpacket jp, char *to)
{
    if (s->connected == 0)
    {
        jp->aux1 = (void *) to;
        mt_jpbuf_en(s->buff,jp,&mt_chat_message_flush,(void *) s);
    }
    else
        mt_chat_send(s,jp,to);
}

void mt_message(session s, jpacket jp)
{
    if (jp->to->user != NULL)
    {
        char *to = mt_jid2mid(jp->p,jp->to);
        if (to == NULL || strcmp(to,s->user) == 0)
        {
            jutil_error(jp->x,TERROR_BAD);
            mt_deliver(s->ti,jp->x);
        }
        else
        {
            lowercase(to);
            if (xmlnode_get_tag(jp->x,"x?xmlns=jabber:x:conference"))
                mt_con_invite(s,jp,to);
            else
                mt_chat_message(s,jp,to);
        }
    }
    else
    {
        jutil_error(jp->x,TERROR_NOTALLOWED);
        mt_deliver(s->ti,jp->x);
    }
}
