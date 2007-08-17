/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 *  Starting and stopping the transport, handling of incoming Jabber packets */

/** @mainpage JIT code documentation
 * For now, simply take a look at the File List. */

#include "icqtransport.h"

result it_sessions_check(void * arg);
void it_shutdown(void *arg);

iconv_t _ucs2utf;
iconv_t _win2utf;
iconv_t _utf2win;

/** Start up transport. Read configuration, register callbacks. */
void icqtrans(instance i, xmlnode x)
{
    iti ti;
    pool p = i->p;
    xmlnode config;
    xmlnode cur;
	int check;

    log_debug(ZONE,"ICQ Transport, initializing for section '%s'",i->id);

    /* create new transport instance */
    ti = pmalloco(p,sizeof(_iti));
    ti->i = i;
    ti->xc = xdb_cache(i);

    config = xdb_get(ti->xc,jid_new(xmlnode_pool(x),"config@-internal"),"jabber:config:icqtrans");
    if (config == NULL)
    {
        log_error(i->id,"Configuration not found!");
        return;
    }

    ti->registration_instructions = pstrdup(p,xmlnode_get_tag_data(config,"instructions"));
    if (ti->registration_instructions == NULL)
    { 
        log_debug(i->id,"Registration instructions not found");
    }

    ti->search_instructions = pstrdup(p,xmlnode_get_tag_data(config,"search"));
    if (ti->search_instructions == NULL)
    {
        log_debug(i->id,"Search instructions not found");
    }

    ti->charset = pstrdup(p,xmlnode_get_tag_data(config,"charset"));
    if (ti->charset == NULL)
    {
	  log_debug(i->id,"Charset not specified, set default to %s ",DEFAULT_CHARSET);
	  ti->charset = pstrdup(p,DEFAULT_CHARSET);
    }

    _ucs2utf = iconv_open("UTF-8","UCS-2BE");

    _win2utf = iconv_open("UTF-8",ti->charset);
    if (_win2utf==(iconv_t)-1) {
      ti->charset = pstrdup(p,DEFAULT_CHARSET);
      _win2utf = iconv_open("UTF-8",ti->charset);
      if (_win2utf==(iconv_t)-1) {
        log_error(i->id,"Charset error!");
        return;
      }
    }

    _utf2win = iconv_open(ti->charset,"UTF-8");
    if (_utf2win ==(iconv_t)-1) {
      ti->charset = pstrdup(p,DEFAULT_CHARSET);
      _utf2win = iconv_open(ti->charset,"UTF-8");
      if (_utf2win ==(iconv_t)-1) {
        log_error(i->id,"Charset error!");
        return;
      }
    }

	log_notice("config","charset %s",ti->charset);
	
    ti->msg_chat = xmlnode_get_tag(config,"chat") ? 1 : 0;
	if (ti->msg_chat) {
	  log_notice("config","chat messages enabled");
	}
    ti->web_aware = xmlnode_get_tag(config,"web") ? 1 : 0;
	if (ti->web_aware) {
	  log_notice("config","web presence enabled");
	}
    ti->own_roster = xmlnode_get_tag(config,"own_roster") ? 1 : 0;
	if (ti->own_roster) {
	  log_notice("config","JIT will use own roster");
	}
    ti->no_jabber_roster = xmlnode_get_tag(config,"no_jabber_roster") ? 1 : 0;
	if (ti->no_jabber_roster) {
	  log_notice("config","JIT willn't get users from jabber roster");
	}
	ti->no_x_data = xmlnode_get_tag(config,"no_xdata") ? 1 : 0;
	if (ti->no_x_data) {
	  log_notice("config","JIT will not use xdata");
	}

    cur = xmlnode_get_tag(config,"sms");

    if (cur) {
      ti->sms_id = pstrdup(p,xmlnode_get_tag_data(cur,"host"));
      if (ti->sms_id) {
        ti->sms_show = jit_show2status(xmlnode_get_tag_data(cur,"show"));

        if (ti->sms_show==ICQ_STATUS_NOT_IN_LIST) {
          ti->sms_show = ICQ_STATUS_ONLINE;
        }
        
        log_notice("config","sms host %s show: %d",ti->sms_id,ti->sms_show);

        ti->sms_status = pstrdup(p,xmlnode_get_tag_data(cur,"status"));

        if (ti->sms_status) {
          log_debug(ZONE,"sms st %s ",ti->sms_status);
        }

        ti->sms_name = pstrdup(p,xmlnode_get_tag_data(cur,"name"));

	if (ti->sms_name) {
	  log_debug(ZONE,"sms name %s",ti->sms_name);
	}
      }
    }

    ti->count_file = pstrdup(p,xmlnode_get_tag_data(config,"user_count_file"));
    if (ti->count_file == NULL)  {
      ti->count_file = "icqcount";
    }

	log_notice("config","Using %s as count log file",ti->count_file);

    for (cur = xmlnode_get_firstchild(xmlnode_get_tag(config,"server"));
     cur != NULL;
     cur = xmlnode_get_nextsibling(cur)) {

      char * port;
      char * host;

      if (xmlnode_get_type(cur) != NTYPE_TAG) continue;

      if ((port = xmlnode_get_attrib(cur,"port")) == NULL) continue;
   
      if ((host = xmlnode_get_data(cur)) == NULL) continue;

      ti->auth_hosts[ti->auth_hosts_count] = pstrdup(p,host);
      ti->auth_ports[ti->auth_hosts_count] = j_atoi(port,5190);
      log_debug(ZONE,"Host %s port %d at pos %d",
				ti->auth_hosts[ti->auth_hosts_count],
				ti->auth_ports[ti->auth_hosts_count],
				ti->auth_hosts_count);
      ti->auth_hosts_count++;

      if (ti->auth_hosts_count >= MAX_AUTH_HOSTS) break;
    }

    if (ti->auth_hosts_count == 0) {
      log_alert("err","No hosts to auth icq client !. Using default");
      ti->auth_hosts[ti->auth_hosts_count] = pstrdup(p,"205.188.179.233");
      ti->auth_ports[ti->auth_hosts_count] = 5190;
      ti->auth_hosts_count++;
    }

	/* add queue for unknown packets */
	ti->q = mtq_new(i->p);

    ti->sessions = wpxhash_new(j_atoi(xmlnode_get_tag_data(config,"prime"),509));
    ti->sessions_alt = wpxhash_new(j_atoi(xmlnode_get_tag_data(config,"prime"),509));
    SEM_INIT(ti->sessions_sem);
    ti->vcard = xmlnode_new_tag_pool(p,"vCard");

    xmlnode_put_attrib(ti->vcard,"xmlns",NS_VCARD);
    xmlnode_insert_node(ti->vcard,xmlnode_get_firstchild(xmlnode_get_tag(config,"vCard")));

    /* default 5 hours */
    ti->session_timeout = j_atoi(xmlnode_get_tag_data(config,"session_timeout"),18000);
	log_notice("config","session_timeout in sec : %d",ti->session_timeout);

    ti->reconnect = j_atoi(xmlnode_get_tag_data(config,"reconnects"),0);
    log_notice("config","Number of reconnects for session %d",ti->reconnect);

	check = j_atoi(xmlnode_get_tag_data(config,"session_check"),10);
    log_notice("config","JIT will check session every %d sec",check);
      
	//    ti->admin = xmlnode_dup(xmlnode_get_tag(config,"admin"));
    ti->start = time(NULL);

    /* Register callbacks */
    register_phandler(i,o_DELIVER,it_receive,(void *) ti);
    register_shutdown(it_shutdown,(void *) ti);

    /* Start up heartbeat thread */
    register_beat(check,it_sessions_check,(void *) ti);

    xmlnode_free(config);
}

/** Callback processing incoming Jabber packets. */
result it_receive(instance i, dpacket d, void *arg) {
    iti ti = (iti) arg;
    jpacket jp;
    session s;
    session_ref alt_s;
    unsigned char *user;
    
    log_debug(ti->i->id,"Packet received: %s\n",xmlnode2str(d->x)); 

    switch(d->type) {
    case p_ROUTE: {
      /* ignore */
      return r_PASS;
    }
            
    case p_NONE:
    case p_NORM:
      jp = jpacket_new(d->x);
      break;

    default:
      return r_ERR;
    }

    if (!jp->from ||/* !jp->from->user ||*/ jp->type == JPACKET_UNKNOWN /* || jpacket_subtype(jp) == JPACKET__ERROR */)
    { /* ignore invalid packets */
        xmlnode_free(jp->x);
        return r_DONE;
    }

    /* JID user part should be case insensitive */
    /* convert user part of from JID to lower case */
    if(jp->from->user != NULL) 
	  for(user = jp->from->user; *user != '\0'; user++)
		if(*user < 128)
		  *user = tolower(*user);
	/* Mangle "from" JID, save original attribute for XDB conversion */
	xmlnode_put_attrib(jp->x, "origfrom", xmlnode_get_attrib(jp->x, "from"));
	xmlnode_put_attrib(jp->x, "from", jid_full(jp->from));

    SEM_LOCK(ti->sessions_sem);
    s = (session) wpxhash_get(ti->sessions,jid_full(jid_user(jp->from)));
    alt_s = (session_ref) wpxhash_get(ti->sessions_alt,jp->to->user);
    if (s != NULL) {
      if (s->exit_flag) {
    SEM_UNLOCK(ti->sessions_sem);
    log_alert("exit flag","message to exiting session");
    if (jp->type != JPACKET_PRESENCE){
      jutil_error(jp->x,TERROR_NOTFOUND);
      it_deliver(ti,jp->x);
    }
    else
      xmlnode_free(jp->x);
      } else if ((alt_s != NULL) && (
               (jp->type == JPACKET_MESSAGE)            // all messages
            || ((jp->type == JPACKET_IQ) && (j_strcmp(xmlnode_get_attrib(jp->iq,"xmlns"),NS_VCARD) == -1)) // all IQs except of vCard
            || (jp->type == JPACKET_PRESENCE) )) {      // all presences _targeted_to_specific_user_
        // rewriting "to" and "from" and putting packet back on the wire
        xmlnode_put_attrib(jp->x, "from", jid_full(it_uin2jid(jp->p,s->uin,jp->to->server)));
        xmlnode_put_attrib(jp->x, "to", jid_full(alt_s->s->orgid));
        SEM_UNLOCK(ti->sessions_sem);
        it_deliver(ti,jp->x);
      } else {
        jp->aux1 = (void *) s;
        mtq_send(s->q,jp->p,it_session_jpacket,(void *) jp);
        SEM_UNLOCK(ti->sessions_sem);
      }
    }
    else {
	  SEM_UNLOCK(ti->sessions_sem);

      if(jpacket_subtype(jp)!=JPACKET__ERROR)
    it_unknown(ti,jp);
      else
    xmlnode_free(jp->x);
    }
    return r_DONE;
}

/** Shut down transport, kill all sessions */
void it_shutdown(void *arg)
{
    iti ti = (iti) arg;

    log_alert(ZONE,"JIT Transport, shutting down");

    ti->shutdown = 1;

    /* wait */
    usleep(1000);

    if (ti->sessions_count) { 
	  SEM_LOCK(ti->sessions_sem);
      wpxhash_walk(ti->sessions,it_sessions_end,NULL);      
	  SEM_UNLOCK(ti->sessions_sem);
    }

    while (ti->sessions_count > 0)
      usleep(100);
    
    wpxhash_free(ti->sessions);
    ti->sessions = NULL;
}
