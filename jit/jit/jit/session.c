/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 *  Starting/stopping user sessions, registration */

#include "icqtransport.h"

void it_session_presence_send(session s);

/** Create new session.
 * @param jp Packet with XDB REGISTER data 
 * leave semaphore up */
session it_session_create(iti ti, jpacket jp)
{
    pool p;
    session s,t;

    log_debug(ZONE,"Creating session for %s",jid_full(jp->from));

    p = pool_heap(4096);
    s = pmalloco(p,sizeof(_session));
    s->p = p;
    s->ti = ti;
    s->q = mtq_new(p);

    s->id = jid_new(p,jid_full(jid_user(jp->from)));
    s->orgid = jid_new(p,jid_full(jp->from));
    jid_full(s->id);
    jid_full(s->orgid);
    s->from = jid_new(p,jid_full(jp->to));
    jid_set(s->from,NULL,JID_RESOURCE);
    jid_full(s->from);

    s->reference_count = 0;
    s->queue = NULL;
    s->queue_last = NULL;
    s->type = stype_normal;

    s->client = NULL;

    s->start_time = time(NULL);
    s->last_time = s->start_time;

    s->web_aware = ti->web_aware;
    
	SEM_LOCK(ti->sessions_sem);
    t = (session) wpxhash_get(ti->sessions,jid_full(s->id));
    if (t != NULL) {
      pool_free(p);
      return NULL;
    }
    wpxhash_put(ti->sessions,pstrdup(p,jid_full(s->id)),(void *) s);
    ti->sessions_count++;
    return s;
}


/** Pass Jabber packet to its corresponding session */
void it_session_jpacket(void *arg)
{
    jpacket jp = (jpacket) arg;
    session s = (session) jp->aux1;

    if (s->exit_flag)
    {
        if (jp->type != JPACKET_PRESENCE)
        {
            jutil_error(jp->x,TERROR_NOTFOUND);
            it_deliver(s->ti,jp->x);
        }
        else
            xmlnode_free(jp->x);
        return;
    }

	/* if connected and no backend */
	if ((s->connected == 1)  && (s->client == NULL)) {
      log_alert(ZONE,"No C++ backend found for this session.");
      xmlnode_free(jp->x);
      return;
	}
    
    s->last_time = time(NULL);

    switch (jp->type)
    {
    case JPACKET_IQ:
      it_iq(s,jp);
      break;
    case JPACKET_PRESENCE:
      it_presence(s,jp);
      break;
    case JPACKET_S10N:
      it_s10n(s,jp);
      break;
    case JPACKET_MESSAGE:
      it_message(s,jp);     
      break;
    default:
      xmlnode_free(jp->x);
    }
}

/** Logged in successfully after initial registration.
 * Put registration information to XDB.
 * Import server based contact list.
 * @param jp Registering information */
void it_session_register(session s, jpacket jp)
{
  iti ti = s->ti;

  s->type = stype_normal;

  if ( it_reg_set(s,jp->iq) == 0) {
        /* data successfully put to XDB */
        xmlnode x;
        char *from;

        log_record("registernew", "", "", ";%s;", 
                   jid_full(s->id));

        from = jid_full(s->from);

        /* make sure we're in the Jabber user's roster */
        x = jutil_presnew(JPACKET__SUBSCRIBE,jid_full(s->id),NULL);
        xmlnode_put_attrib(x,"from",from);
        it_deliver(ti,x);

        x = jutil_presnew(JPACKET__PROBE,jid_full(s->id),NULL);
        xmlnode_put_attrib(x,"from",from);
        it_deliver(ti,x);
        
        jutil_iqresult(jp->x);
        it_deliver(ti,jp->x);

        /* Now request server contact list */
        FetchServerBasedContactList(s);
  }
  else   {
    jutil_error(jp->x,(terror){500,"XDB troubles"});
    it_deliver(ti,jp->x);
    if (s->exit_flag)
      return;
    EndClient(s); // will call it_session_end
  }
}

/** Session thread, called from wp_client when connected */
void it_session_confirmed(session s){
  queue_elem queue;
  jpacket jp;
  
  if (s->exit_flag) return;
  
  s->start_time = time(NULL);
    
  log_record("sessionstart", "", "", ";%s;", 
             jid_full(s->id));

  queue = QUEUE_GET(s->queue,s->queue_last);
  jp = queue->elem;

  if (s->type != stype_normal) {
    it_session_register(s,jp);
      
    if (s->exit_flag)
      return;
  }
  else {
    if (jp->type == JPACKET_PRESENCE) {    
      /* send actual presence state to Jabber */
      it_session_presence_send(s);
      /* send actual presence state to ICQ */
      SendStatus(s);
    }        
    else {
      log_alert("debug","Internal error!");
    }
    xmlnode_free(jp->x);
  }

  /* process enqueued packets */

  if(s->exit_flag) return;

  s->connected = 1;

  if ((s->ti->own_roster)&&(s->roster_changed)) {
	it_save_contacts(s);
	s->roster_changed = 0;
  }

  /* set sms contacts presence */
  it_sms_presence(s,1);
    
  /* flush buffer of packets waiting for login to finish */
  while ((queue = QUEUE_GET(s->queue,s->queue_last)) != NULL)
  {
    jp = queue->elem;

    /* JPACKET_PRESENCE is never buffered */
    switch (jp->type)
    {
      case JPACKET_IQ:
        it_iq(s,jp);
        break;
      case JPACKET_S10N:
        it_s10n(s,jp);
        break;
      case JPACKET_MESSAGE:
        it_message(s,jp);     
        break;

      default:
        xmlnode_free(jp->x);
    }

    /* if got error, do not process more packetc */
    if(s->exit_flag) return;
  }
}

/** Free session if not referenced anymore */
result session_free(void * arg) {
  if (((session)arg)->reference_count) {
	log_alert(ZONE,"Strange. Ref > 0");
    return r_DONE;
  }

  log_debug(ZONE,"session free");

  pool_free(((session)arg)->p);
  return r_UNREG;
}

result session_reconnect(void * arg) {
  xmlnode pres = (xmlnode)arg;

  deliver(dpacket_new(pres),NULL);
  return r_UNREG;
}

/** Reconnect if configured to do so */
void it_session_free(void *arg)
{
  session s = (session) arg;

  if ((s->reconnect)&&(s->reconnect_count < s->ti->reconnect)) {
    xmlnode pres;
	const char * show;
    char buf[10];
    s->reconnect_count++;
    
    log_alert(ZONE,"Reconnect %d for user %s",s->reconnect_count,jid_full(s->id));
    
    pres = jutil_presnew(JPACKET__AVAILABLE,jid_full(s->from),NULL);
    if(s->status_text[0])
	  xmlnode_insert_cdata(xmlnode_insert_tag(pres,"status"),
						   s->status_text,strlen(s->status_text));

	show = jit_status2show(s->status);
	if (show != NULL)
	  xmlnode_insert_cdata(xmlnode_insert_tag(pres,"show"),
						   show,strlen(show));
	
    xmlnode_put_attrib(pres,"from",jid_full(s->orgid));
    sprintf(buf,"%d",s->reconnect_count);
    xmlnode_put_attrib(pres,"reconnect",buf);
	
	register_beat(5,session_reconnect,(void *)pres);	
	//    it_deliver(s->ti,pres);
  }
  
  s->exit_flag = 2;
  
  register_beat(120,session_free,s);
}

/** Shutdown session, free resources etc */
void it_session_exit(void *arg)
{
    session s = (session) arg;
    iti ti = s->ti;
    queue_elem queue;

    log_debug(ZONE,"Session[%s], exiting",jid_full(s->id));

    if (s->client != NULL) {
      /* free C++ if error from C++ */
      EndClient(s);
    }

	s->client = NULL;

    /* free mio */
    if (s->s_mio) {
      mio_close(s->s_mio);
      s->s_mio = NULL;
    }

    while ((queue = QUEUE_GET(s->queue,s->queue_last)) != NULL)
    {
      xmlnode x = ((jpacket)(queue->elem))->x;
      if (((jpacket)(queue->elem))->type == JPACKET_PRESENCE) {
        xmlnode_free(x);
      }
      else {
        jutil_error(x,TERROR_NOTFOUND);
        it_deliver(ti,x);
      }
    }
    
    s->queue = NULL;
    s->queue_last = NULL;

    ppdb_free(s->p_db);

    if (s->contacts)
      it_contact_free(s);

    if (s->pend_search) {
      pool_free(s->pend_search->p);
      s->pend_search = NULL;
    }

    if (s->vcard_get) {
      pool_free(s->vcard_get->jp->p);
      s->vcard_get = NULL;
    }
    
    mtq_send(s->q,s->p,it_session_free,(void *) s);
}

/** Registration error */
void it_session_regerr(session s, terror e)
{
  queue_elem queue;
  xmlnode x;

  log_alert(ZONE,"Session reg error");

  /* get IQ */
  queue = QUEUE_GET(s->queue,s->queue_last);
  x = ((jpacket)(queue->elem))->x;

  jutil_error(x,e);
  it_deliver(s->ti,x);
}

/** Send session's presence to Jabber */
void it_session_presence_send(session s) 
{ 
  xmlnode pres,x;

  pres = jutil_presnew(JPACKET__AVAILABLE,jid_full(s->id),s->status_text);

  xmlnode_put_attrib(pres,"from",jid_full(s->from));

  switch (s->status) 
    { 
    case ICQ_STATUS_ONLINE:
      break;
    case ICQ_STATUS_AWAY: 
      x = xmlnode_insert_tag(pres,"show"); 
      xmlnode_insert_cdata(x,"away",-1); 
      break; 
    case ICQ_STATUS_DND:
       x = xmlnode_insert_tag(pres,"show");
       xmlnode_insert_cdata(x,"dnd",-1);
       break;
    case ICQ_STATUS_NA:
       x = xmlnode_insert_tag(pres,"show");
       xmlnode_insert_cdata(x,"xa",-1);
       break;
    case ICQ_STATUS_OCCUPIED:
       x = xmlnode_insert_tag(pres,"show");
       xmlnode_insert_cdata(x,"dnd",-1);
       break;
    case ICQ_STATUS_FREE_CHAT:
       x = xmlnode_insert_tag(pres,"show");
       xmlnode_insert_cdata(x,"chat",-1);
       break;

    default:
      break;
    }
     
  it_deliver(s->ti,pres);
}

/** Send session unavailable presence to Jabber */
void it_session_unavail(session s, char *msg)
{
    xmlnode pres;

    pres = jutil_presnew(JPACKET__UNAVAILABLE,jid_full(s->id),NULL);
    xmlnode_put_attrib(pres,"from",jid_full(s->from));
    xmlnode_insert_cdata(xmlnode_insert_tag(pres,"status"),msg,-1);
    it_deliver(s->ti,pres);

    /* set sms contacts */
    it_sms_presence(s,0);
}

/** Regular exit (i.e. user sent 'unavailable' or asked to remove his registration */
void it_session_end(session s)
{
    if (s->exit_flag)
        return;

    log_debug(ZONE,"Killing session[%s]",jid_full(s->id));

    s->exit_flag = 1;

    if (s->type != stype_normal) { 
      it_session_regerr(s,TERROR_NOTACCEPTABLE);
    }
    else {
      it_session_unavail(s,"Disconnected");
      log_record("sessionend", "", "", ";%s;%d", 
         jid_full(s->id),
         time(NULL)-s->start_time);
    }
    
    if ((s->ti->own_roster)&&(s->roster_changed)) {
      it_save_contacts(s);
      s->roster_changed = 0;
    }

    /* remove from hash */
	SEM_LOCK(s->ti->sessions_sem);
    if (s->uin) {
      char buffer[16];
      snprintf(buffer,16,"%lu",s->uin);

      if ((session) wpxhash_get(s->ti->sessions_alt,buffer) != NULL)
        wpxhash_zap(s->ti->sessions_alt,buffer);
    };
    wpxhash_zap(s->ti->sessions,jid_full(s->id));
    s->ti->sessions_count--;
	SEM_UNLOCK(s->ti->sessions_sem);

    mtq_send(s->q,s->p,it_session_exit,(void *) s);
    /* here client class is freed */
}

/** Handling of session errors */
void it_session_error(session s, terror e)
{
  if (s->exit_flag)
    return;            

  s->exit_flag = 1;

  if(s->type == stype_normal) {
    it_session_unavail(s,e.msg);

    log_record("sessionend", "", "", ";%s;%d", 
           jid_full(s->id),
           time(NULL)-s->start_time);
    
    /* reconnect if network error or unknown error or turboing */
    if (e.code == 502 || e.code==503) {

      /* if session time > 5 min reset reconnect_count */
      if ((time(NULL) - s->start_time) > 60*5) 
        s->reconnect_count = 0;

      /* it_session_free will take care of reconnection attempt */
      s->reconnect = 1;
    }   
  }
  else 
    it_session_regerr(s,e);

    if ((s->ti->own_roster)&&(s->roster_changed)) {
      it_save_contacts(s);
      s->roster_changed = 0;
    }

  /* remove from hash */
  SEM_LOCK(s->ti->sessions_sem);
  if (s->uin) {
    char buffer[16];
    snprintf(buffer,16,"%lu",s->uin);

    if ((session) wpxhash_get(s->ti->sessions_alt,buffer) != NULL)
      wpxhash_zap(s->ti->sessions_alt,buffer);
  };
  wpxhash_zap(s->ti->sessions,jid_full(s->id));
  s->ti->sessions_count--;
  SEM_UNLOCK(s->ti->sessions_sem);

  /* here client class is freed */
  mtq_send(s->q,s->p,it_session_exit,(void *) s);
}

/** Helper for heartbeat xhash walking function, session thread */
void it_session_check_rcv(void *arg) {
  session s = (session) arg;
  int now;

  if (s->exit_flag) 
    return;

  SessionCheck(s);

  now = time(NULL);
  if (s->ti->session_timeout) {
    if ((now - s->last_time) > s->ti->session_timeout ) {
      log_alert(ZONE,"Session [%s] timedout",jid_full(s->id));
      if (s->exit_flag) 
        return;
      EndClient(s);
    }
  }
}

/** Heartbeat thread, xhash walking function */
void it_session_check_walker(wpxht h, const char *key, void *val, void *arg) {
  session s = (session) val;

  if (s->exit_flag) return;
  if (s->connected == 0) return;
  mtq_send(s->q,NULL,it_session_check_rcv,val);
}

/** Check sessions xhash walk (heartbeat thread) */
result it_sessions_check(void * arg) {
  iti ti = (iti) arg;

  if (ti->shutdown == 1)
    return r_DONE;

  SEM_LOCK(ti->sessions_sem);
  wpxhash_walk(ti->sessions,it_session_check_walker,NULL); 
  SEM_UNLOCK(ti->sessions_sem);

  /* write count to log file */
  if (ti->count_file) {
    FILE* f=fopen(ti->count_file,"w+");
    if (f) {
      fprintf(f,"%u",ti->sessions_count);
      fclose(f);
    }
  }

  return r_DONE;
}

/** Kill session (session thread) */
void it_session_kill(void *arg) {
  session s = (session)arg;
  if (s->exit_flag) 
    return;
  EndClient(s);
}

/** Kill sessions (main thread) */
void it_sessions_end(wpxht h, const char *key, void *val, void *arg) 
{
  session s = (session)val;
  mtq_send(s->q,s->p,it_session_kill,val);
}



