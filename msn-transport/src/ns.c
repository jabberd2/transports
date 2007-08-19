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


void mt_ns_rng(mpacket mp, session s)
{
    sbchat sc;
    char *user = mt_packet_data(mp,5);
    char *sid, *chal, *host, *port;

    sc = xhash_get(s->chats,user);  /* is there already is a SB session? */
    if (sc != NULL)
    {
        if (sc->state == sb_START)
        {
            log_debug(ZONE,"SB Session with '%s' already started",user);
            return;
        }

        log_debug(ZONE,"Replacing SB session");
        mt_chat_end(sc);
    }

    sid = mt_packet_data(mp,1);
    host = mt_packet_data(mp,2);
    chal = mt_packet_data(mp,4);
    port = strchr(host,':');

    if (port != NULL)
    {
        *port = '\0';
        port++;
    }

    mt_chat_join(s,user,host,j_atoi(port,1863),chal,sid);
}

void mt_ns_not(mpacket mp, session s)
{
    int i = 0;
    xmlnode msg, oob1, oob2;
    /* message body spool*/
    pool p = pool_new();
    spool sp = spool_new(p);
    /* parsing the message... */
    xmlnode notification, msgtag, action, subscr, body, text;
    spool action_url = spool_new(p);
    spool subscr_url = spool_new(p);
    char *chunk, *fixedchunk, *notification_id, *msg_id, *bodytext;

    if (s->ti->inbox_headlines == 0)
        return;

    /* grab the <notification/> chunk */
    for(i = 2; i < mp->count; i++) {
        spool_add(sp, mt_packet_data(mp,i));
    }

    msg = xmlnode_new_tag("message");
    xmlnode_put_attrib(msg,"to",jid_full(s->id));
    xmlnode_put_attrib(msg,"from",s->host);
    xmlnode_put_attrib(msg,"type","headline");

    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"subject"),"MSN Alert",-1);

    /* Parse the alert -- there's probably better ways to do this */
    chunk = spool_print(sp);
    log_debug(ZONE, "chunk from spool_print: \"%s\"", chunk);
    fixedchunk = mt_fix_amps(p, chunk);
    log_debug(ZONE, "fixedchunk: \"%s\"", fixedchunk);
    // Get the notification ID
    notification = xmlnode_str(fixedchunk, strlen(fixedchunk));
    notification_id = xmlnode_get_attrib(notification, "id");
    log_debug(ZONE, "notification - %X\nn_id - %s", notification, notification_id);
    // Get the message ID
    msgtag = xmlnode_get_tag(notification, "MSG");
    msg_id = xmlnode_get_attrib(msgtag, "id");
    // Get the action URL
    action = xmlnode_get_tag(msgtag, "ACTION");
    spool_add(action_url, xmlnode_get_attrib(action, "url"));
    spool_add(action_url, "&notification=");
    spool_add(action_url, notification_id);
    spool_add(action_url, "&message_id=");
    spool_add(action_url, msg_id);
    spool_add(action_url, "&agent=messenger");
    // Get the subscription URL
    subscr = xmlnode_get_tag(msgtag, "SUBSCR");
    spool_add(subscr_url, xmlnode_get_attrib(subscr, "url"));
    spool_add(subscr_url, "&notification=");
    spool_add(subscr_url, notification_id);
    spool_add(subscr_url, "&message_id=");
    spool_add(subscr_url, msg_id);
    spool_add(subscr_url, "&agent=messenger");
    // Get the body
    body = xmlnode_get_tag(msgtag, "BODY");
    text = xmlnode_get_tag(body, "TEXT");
    bodytext = xmlnode_get_data(text);
    /* Finished parsing the XML */

    // Insert the body
    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),bodytext,-1);
    
    // Insert the action URL
    oob1 = xmlnode_insert_tag(msg,"x");
    xmlnode_put_attrib(oob1,"xmlns","jabber:x:oob");
    xmlnode_insert_cdata(xmlnode_insert_tag(oob1,"url"),spool_print(action_url),-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(oob1,"desc"),"More information on this alert",-1);
    
    // Insert the subscription URL
    oob2 = xmlnode_insert_tag(msg,"x");
    xmlnode_put_attrib(oob2,"xmlns","jabber:x:oob");
    xmlnode_insert_cdata(xmlnode_insert_tag(oob2,"url"),spool_print(subscr_url),-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(oob2,"desc"),"Manage subscriptions to alerts",-1);

    mt_deliver(s->ti,msg);
    
    xmlnode_free(notification);
    
    pool_free(p);   
}

void mt_ns_msg(mpacket mp, session s)
{
    xmlnode msg, oob;
    char *body, *ctype, *ptr;
    /* message body spool*/
    pool p = pool_new();
    spool sp = spool_new(p);      

    if (s->ti->inbox_headlines == 0)
        return;

    ctype = strchr(mt_packet_data(mp,5),':') + 2;
    body = mt_packet_data(mp,mp->count - 1);

    /* this message is a Hotmail inbox notification */
    if ((strncmp(ctype,"text/x-msmsgsinitialemailnotification",37) != 0) &&
        (strncmp(ctype,"text/x-msmsgsemailnotification",30) != 0))
        return;
   
    /* Fede <vulcano@lugmen.org.ar> */
    /* cut off the junk at the end */
    if ((ptr = strstr(body,"Inbox-URL")) != NULL) {
       *ptr = '\0';
       spool_add(sp,body);   
    } else {       
       if ((ptr = strstr(body,"From:")) != NULL) {
          char *p = strchr(ptr, '\r');	  
	  *p = '\0';
	  spooler(sp,"Mail from: ", ptr + 6,sp);
	  body = p + 1;
       }
       if ((ptr = strstr(body,"From-Addr:")) != NULL) {
          *strchr(ptr, '\r') = '\0';
	  spooler(sp," <",ptr + 11,">",sp);
       }       
    }
    
    msg = xmlnode_new_tag("message");
    xmlnode_put_attrib(msg,"to",jid_full(s->id));
    xmlnode_put_attrib(msg,"from",s->host);
    xmlnode_put_attrib(msg,"type","headline");

    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"subject"),"Hotmail",-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),spool_print(sp),-1);

    oob = xmlnode_insert_tag(msg,"x");
    xmlnode_put_attrib(oob,"xmlns","jabber:x:oob");
    xmlnode_insert_cdata(xmlnode_insert_tag(oob,"url"),"http://www.hotmail.com/cgi-bin/folders",-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(oob,"desc"),"Login to your Hotmail e-mail account",-1);

    mt_deliver(s->ti,msg);
   
    pool_free(p);
}

void mt_ns_iln(mpacket mp, session s)
{
    mt_user_update(s,mt_packet_data(mp,3),
                   mt_packet_data(mp,2),
                   mt_packet_data(mp,4));
}

void mt_ns_nln(mpacket mp, session s)
{
    mt_user_update(s,mt_packet_data(mp,2),
                   mt_packet_data(mp,1),
                   mt_packet_data(mp,3));
}

void mt_ns_fln(mpacket mp, session s)
{
    muser u;

    u = mt_user(s,mt_packet_data(mp,1));
    u->state = ustate_fln;
    mt_user_sendpres(s,u);
}

result mt_ns_add(mpacket mp, void *arg)
{
    session s = (session) arg;
    char *cmd = mt_packet_data(mp,0);

    if (j_strcmp(cmd,"ADD") != 0)
    {
        if (j_strcmp(cmd,"ILN") == 0)
        {
            mt_ns_iln(mp,s);
            return r_PASS;
        }
    }
    else if (j_strcmp(mt_packet_data(mp,2),"RL") == 0)
    {
        char *user = mt_packet_data(mp,4);

        if (user != NULL)
        {
            muser u;

            u = mt_user(s,user);
            u->list = u->list | LIST_RL;
            mt_user_subscribe(s,u);
            return r_DONE;
        }
    }
    return r_ERR;
}

result mt_ns_rem(mpacket mp, void *arg)
{
    if (j_strcmp(mt_packet_data(mp,0),"REM") == 0 && j_strcmp(mt_packet_data(mp,2),"RL") == 0)
    {
        session s = (session) arg;
        char *user = mt_packet_data(mp,4);

        if (user != NULL)
        {
            muser u = mt_user(s,user);
            u->list ^= LIST_RL;
            mt_user_unsubscribe(s,u);
            return r_DONE;
        }
    }
    return r_ERR;
}

result mt_ns_usr_P(mpacket mp, void *arg)
{
    session s = (session) arg;
    char *cmd = mt_packet_data(mp,0);

    if (j_strcmp(cmd,"USR") == 0) /* auth was successful */
    {
        log_debug(ZONE,"Auth successful for '%s' ",s->user);

        if (s->type == stype_register)
        {
            jpacket jp = mt_jpbuf_de(s->buff);
            s->type = stype_normal;
            jp->aux1 = (void *) s;
            mtq_send(s->q,jp->p,&mt_reg_success,(void *) jp);
        }
        else
            mt_user_sync(s);
    }
    else if (j_atoi(cmd,0) == 911)
        mt_session_kill(s,TERROR_AUTH);
    else if (j_atoi(cmd,0) != 0)
    {
        mt_ns_close(s);
        mt_ns_reconnect(s);     /* some other wierd error, try again */
    }
    else
        return r_ERR;

    return r_DONE;
}

void mt_ns_xfr(mpacket mp, session s)
{
    if (j_strcmp(mt_packet_data(mp,2),"NS") == 0)
    {
        char *host = mt_packet_data(mp,3);
        char *port = strchr(host,':');

        if (port != NULL)
        {
            *port = '\0';
            ++port;
        }

        mt_ns_close(s);
        mt_ns_connect(s,host,j_atoi(port,1863));
    }
    else
    {
        log_debug(ZONE,"Session[%s], NS XFR Error, %s",jid_full(s->id),mt_packet_data(mp,2));
    }
}

result mt_ns_usr_I(mpacket mp, void *arg)
{
    session s = (session) arg;
    char *cmd =  mt_packet_data(mp,0);

    if (j_strcmp(cmd,"USR") == 0)
    {
        char tp[500];
        char *authdata = mt_packet_data(mp,4);

        mt_ssl_auth(s, authdata, tp);

        if(strcmp(tp, "") == 0) {
            mt_session_kill(s,TERROR_EXTERNAL);
            return r_ERR;
        }

        mt_stream_register(s->st,&mt_ns_usr_P,(void *) s);
        mt_cmd_usr_P(s->st,tp);
    }
    else if (j_strcmp(cmd,"XFR") == 0)
        mt_ns_xfr(mp,s);
    else if (j_atoi(cmd,0) == 911)
        mt_session_kill(s,(terror) {406,"Invalid Username"});/* 911 at this point means the username doesn't exit */
    else if (j_atoi(cmd,0) != 0)
        mt_session_kill(s,TERROR_EXTERNAL);
    else
        return r_ERR;

    return r_DONE;
}

result mt_ns_cvr(mpacket mp, void *arg)
{
    session s = (session) arg;

    if (j_strcmp(mt_packet_data(mp,0),"CVR") == 0)
    {
        mt_stream_register(s->st,&mt_ns_usr_I,(void *) s);
        mt_cmd_usr_I(s->st,s->user);
        return r_DONE;
    }
    return r_ERR;
}

result mt_ns_ver(mpacket mp, void *arg)
{
    session s = (session) arg;
    char *cmd = mt_packet_data(mp,0);

    if (j_strcmp(cmd,"VER") == 0)
    {
        mt_stream_register(s->st,&mt_ns_cvr,(void *) s);
        mt_cmd_cvr(s->st, s->user);
    }
    else if (j_atoi(cmd,0) != 0)
    {
        /* The server is probably down */
        log_debug(ZONE,"Session[%s], Error code %s, retrying",jid_full(s->id),cmd);
        mt_ns_close(s);
        mt_ns_reconnect(s);
    }
    else
        return r_ERR;

    return r_DONE;
}

void mt_ns_end_rooms(xht h, const char *key, void *val, void *arg)
{
    sbroom r = (sbroom) val;

    if (r->st == NULL)
    { /* free any sessions waiting for an XFR now, since it won't be received */
        mt_con_remove(r);
        mt_con_free(r);
    }
    else
        mt_con_end(r);
}

void mt_ns_end_chats(xht h, const char *key, void *val, void *arg)
{
    sbchat sc = (sbchat) val;

    if (sc->st == NULL)
    {
        mt_chat_remove(sc);
        mt_chat_free(sc);
    }
    else
        mt_chat_end(sc);
}

void mt_ns_end_sbs(session s)
{
    if (s->chats != NULL)
        xhash_walk(s->chats,&mt_ns_end_chats,NULL);

    if (s->rooms != NULL)
        xhash_walk(s->rooms,&mt_ns_end_rooms,NULL);
}

void mt_ns_chl(mpacket mp, void *arg)
{
    session s = (session) arg;
    char *chalstr;
    char result[50];

    chalstr = mt_packet_data(mp,2);
    mt_md5hash(chalstr, "VT6PX?UQTM4WM%YR", result);

    mt_cmd_qry(s->st, result);
}

result mt_ns_packets(mpacket mp, void *arg)
{
    session s = (session) arg;

    if (mp == NULL)
    {
        s->connected = 0;
        s->friendly_flag = 1;
        s->st = NULL;
        mt_ns_end_sbs(s);

        if (s->exit_flag == 0)
        {
            log_debug(ZONE,"Session[%s], MSN server connection closed",jid_full(s->id));
            mt_ns_reconnect(s);
        }

        SREF_DEC(s);
    }
    else if (s->exit_flag == 0)
    {
        char *cmd = mt_packet_data(mp,0);

        if (j_strcmp(cmd,"NLN") == 0)
            mt_ns_nln(mp,s);
        else if (j_strcmp(cmd,"FLN") == 0)
            mt_ns_fln(mp,s);
        else if (j_strcmp(cmd,"ADD") == 0)
            mt_ns_add(mp,arg);
        else if (j_strcmp(cmd,"REM") == 0)
            mt_ns_rem(mp,arg);
        else if (j_strcmp(cmd,"RNG") == 0)
            mt_ns_rng(mp,s);
        else if (j_strcmp(cmd,"XFR") == 0)
            mt_ns_xfr(mp,s);
        else if (j_strcmp(cmd,"MSG") == 0)
            mt_ns_msg(mp,s);
        else if (j_strcmp(cmd,"NOT") == 0)
            mt_ns_not(mp,s);
        else if (j_strcmp(cmd,"ILN") == 0)
            mt_ns_iln(mp,s);
        else if (j_strcmp(cmd,"LST") == 0)
            mt_user_syn(mp,s);
        else if (j_strcmp(cmd,"CHL") == 0)
            mt_ns_chl(mp,s);
        else if (j_strcmp(cmd,"QRY") == 0)
            ; // This can be safely ignored, it's just a response to our challenge response (in the form QRY msg_id)
	else if (j_strcmp(cmd,"PRP") == 0)
            ; // To do with phone numbers - ignore
	else if (j_strcmp(cmd,"BRP") == 0)
            ; // To do with phone numbers - ignore
	else if (j_strcmp(cmd,"LSG") == 0)
            ; // To do with groups on the MSN server, we have no way of using this info, ignore
	else if (j_strcmp(cmd,"GTC") == 0)
            ; // To do with privacy settings - ignore
	else if (j_strcmp(cmd,"BLP") == 0)
            ; // To do with privacy settings - ignore
        else if (j_strcmp(cmd,"OUT") == 0)
        {
            s->connected = 0;
            s->friendly_flag = 1;
            if (j_strcmp(mt_packet_data(mp,1),"OTH") == 0)
                mt_session_kill(s,(terror){409,"Session was replaced"});
            else
                mt_session_kill(s,TERROR_EXTERNAL);
        }
        else if (j_strcmp(cmd,"REA") && j_strcmp(cmd,"CHG"))
            return r_ERR;
    }

    return r_DONE;
}

result mt_ns_closed(mpacket mp, void *arg)
{
    if (mp == NULL)
    {
        session s = (session) arg;
        mt_ns_end_sbs(s);
        SREF_DEC(s);
    }
    return r_DONE;
}

void mt_ns_close(session s)
{
    mpstream st = s->st;

    s->st = NULL;
    s->connected = 0;
    s->friendly_flag = 1;
    st->cb = &mt_ns_closed;
    mt_stream_close(st);
}

void mt_ns_reconnect(session s)
{
    log_debug(ZONE,"Session[%s] reconnecting",jid_full(s->id));

    if (s->attempts < s->ti->attempts_max)
    {
        ++s->attempts;
        mt_ns_start(s);
    }
    else
    {
        log_debug(ZONE,"Session[%s], connection attempts reached max",jid_full(s->id));
        mt_session_kill(s,TERROR_EXTERNAL);
    }
}

void mt_ns_connect(session s, char *server, int port)
{
    mti ti = s->ti;
    mpstream st;

    assert(s->st == NULL);

    log_debug(ZONE,"Session[%s], connecting to NS %s",jid_full(s->id),server);

    SREF_INC(s);

    st = s->st = mt_stream_connect(server,port,&mt_ns_packets,(void *) s);
    mt_stream_register(st,&mt_ns_ver,(void *) s);
    mt_cmd_ver(st);
}

void mt_ns_start(session s)
{
/*    mti ti = s->ti;
    char *server = ti->servers[++ti->cur_server];

    if (server == NULL)
    {
        server = ti->servers[0];
        ti->cur_server = 0;
    }

    mt_ns_connect(s,server,1863);
*/
    mt_ns_connect(s,"messenger.hotmail.com",1863);
}
