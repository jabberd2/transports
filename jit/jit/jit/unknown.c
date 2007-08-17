/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 *  Handling of Jabber packets coming from unregistered (unknown) JIDs */

#include "icqtransport.h"

void it_unknown_iq(iti ti, jpacket jp);
void it_unknown_presence(void *arg);
void it_unknown_reg_get(iti ti, jpacket jp);
void it_unknown_reg_set(iti ti, jpacket jp);


/** function that moves reg query to mtq thread */
void it_unknown_reg_get_mtq(void *arg)
{
  jpacket jp = (jpacket) arg;
  it_unknown_reg_get((iti)jp->aux1,jp);
}

/** Got packet from JID that does not yet have a session */
void it_unknown(iti ti, jpacket jp) {
  log_debug(ZONE,"it_unknown");
  switch (jp->type) {
  case JPACKET_IQ:
    it_unknown_iq(ti,jp);
    return;
    
  case JPACKET_MESSAGE:
  case JPACKET_S10N:
    jp->aux1 = (void *) ti;
    mtq_send(ti->q,jp->p,it_unknown_bounce,(void *) jp);
    return;
    
  case JPACKET_PRESENCE:
    if ( ( (jpacket_subtype(jp) == JPACKET__AVAILABLE) ||
       (jpacket_subtype(jp) == JPACKET__INVISIBLE) ) && 
     (jp->to->user == NULL) )  {
      
      jp->aux1 = (void *) ti;
      mtq_send(ti->q,jp->p,it_unknown_presence,(void *) jp);
      return;
    }
  }

  xmlnode_free(jp->x);
}

/** Got IQ packet from JID that does not yet have a session */
void it_unknown_iq(iti ti, jpacket jp)
{
    char *ns;

    if (jp->to->user != NULL)
    {
        jp->aux1 = (void *) ti;
        mtq_send(ti->q,jp->p,it_unknown_bounce,(void *) jp);
        return;
    }

    ns = xmlnode_get_attrib(jp->iq,"xmlns");

    switch (jpacket_subtype(jp))
    {
    case JPACKET__SET:
	  if (j_strcmp(ns,NS_REGISTER) == 0)
            it_unknown_reg_set(ti,jp);
        else
        {
            jutil_error(jp->x,TERROR_NOTALLOWED);
            it_deliver(ti,jp->x);
        }
        break;

    case JPACKET__GET:
	    if (j_strcmp(ns,NS_REGISTER) == 0) {
		    jp->aux1 = (void *) ti;
		    mtq_send(ti->q,jp->p,it_unknown_reg_get_mtq,(void *) jp);
		}
		else if (j_strcmp(ns,NS_BROWSE) == 0)
            it_iq_browse_server(ti,jp);
        else if (j_strcmp(ns,NS_VERSION) == 0)
            it_iq_version(ti,jp);
        else if (j_strcmp(ns,NS_TIME) == 0)
            it_iq_time(ti,jp);
        else if (j_strcmp(ns,NS_VCARD) == 0)
            it_iq_vcard_server(ti,jp);
	else if (j_strcmp(ns,NS_DISCO_ITEMS) == 0)
	    it_iq_disco_items_server(ti,jp);
	else if (j_strcmp(ns,NS_DISCO_INFO) == 0)
	    it_iq_disco_info_server(ti,jp);
        else if (j_strcmp(ns,NS_LAST) == 0)
            jp->to->user == NULL ? it_iq_last_server(ti,jp) : xmlnode_free(jp->x);
        else
        {
            jutil_error(jp->x,TERROR_NOTALLOWED);
            it_deliver(ti,jp->x);
        }
        break;

    default:
        jutil_error(jp->x,TERROR_NOTALLOWED);
        it_deliver(ti,jp->x);
    }
}

/** Got presence from JID that does not yet have a session.
 * Get registration info from XDB and try to start session. */
void it_unknown_presence(void *arg)
{
    jpacket jp = (jpacket) arg;
    iti ti = (iti) jp->aux1;
    xmlnode reg;
    session s;
    queue_elem queue;
    UIN_t uin;
    char * passwd;

    /* Get auth info from XDB */
    reg = xdb_get(ti->xc,it_xdb_id(jp->p,jp->from,jp->to->server),NS_REGISTER);
    if(reg == NULL)
    {
      /* auth data not found, perhaps pre-2003 non-lowercase XDB format */
      it_xdb_convert(ti, xmlnode_get_attrib(jp->x,"origfrom"), jp->from);
      reg = xdb_get(ti->xc,it_xdb_id(jp->p,jp->from,jp->to->server),NS_REGISTER);
    }

    if (reg == NULL)
    {
        /* we really should send an error message back here */
        log_debug(ZONE,"Registration not found for %s",jid_full(jp->from));
        xmlnode_free(jp->x);
        return;
    }

    uin = it_strtouin(xmlnode_get_tag_data(reg,"username"));
    passwd = xmlnode_get_tag_data(reg,"password");
    
    if (uin == 0 || passwd == NULL)  {
      log_warn(ti->i->id,"User %s has invalid registration settings",jid_full(jp->from));
      xmlnode_free(reg);
      xmlnode_free(jp->x);
      return;
    }

    /* if data ok start session */
    s = it_session_create(ti,jp);
    if (s==NULL) {
	  s = (session) wpxhash_get(ti->sessions,jid_full(jid_user(jp->from)));

	  if (s==NULL) {
		SEM_UNLOCK(ti->sessions_sem);
		log_alert(ZONE,"session is gone");
		xmlnode_free(reg);
		xmlnode_free(jp->x);
		return;
	  }

	  log_debug(ZONE,"Session %s already created ",jid_full(jp->from));
	  jp->aux1 = (void *) s;
	  mtq_send(s->q,jp->p,it_session_jpacket,(void *) jp);
	  SEM_UNLOCK(ti->sessions_sem);

	  xmlnode_free(reg);
	  return;
    }
	
    s->type = stype_normal;
    s->uin = uin;
    if (s->uin) {
      session_ref s_alt;
      char buffer[16];
      s_alt=pmalloco(s->p,sizeof(_session_ref));
      s_alt->s=s;
      snprintf(buffer,16,"%lu",s->uin);
      wpxhash_put(ti->sessions_alt,pstrdup(s->p,buffer),(void*)s_alt);
    };

    s->passwd = it_convert_utf82windows(s->p,passwd);	
    if (strlen(s->passwd)>8) s->passwd[8]='\0';
	
    xmlnode_free(reg);
    
    s->reconnect_count = j_atoi(xmlnode_get_attrib(jp->x,"reconnect"),0);
    
    /* update users presence */
    s->p_db = ppdb_insert(s->p_db,jp->from,jp->x);

    s->status = jit_show2status(xmlnode_get_tag_data(jp->x,"show"));

    /* add to queue */
    queue = pmalloco(jp->p,sizeof(_queue_elem));
    queue->elem = (void *)jp;
    /* next = NULL */

    QUEUE_PUT(s->queue,s->queue_last,queue);

    /* connect auth server */
    StartClient(s);

	SEM_UNLOCK(ti->sessions_sem);
    return;
    
}

/** Send info on how to register to Jabber */
void it_unknown_reg_get(iti ti, jpacket jp)
{
    if (ti->registration_instructions)
    {
	  xmlnode q,x,reg;
	  char *key;

	  /* retur packet */
	  jutil_iqresult(jp->x);
	  q = xmlnode_insert_tag(jp->x,"query");
	  xmlnode_put_attrib(q,"xmlns",NS_REGISTER);

	  /* get reg data */
	  reg = xdb_get(ti->xc,it_xdb_id(jp->p,jp->from,jp->to->server),NS_REGISTER);
	  if (reg != NULL) {
		xmlnode_insert_node(q,xmlnode_get_firstchild(reg));
		xmlnode_free(reg);
		
		/* remove old tags */
		xmlnode_hide(xmlnode_get_tag(q,"nick"));
		xmlnode_hide(xmlnode_get_tag(q,"first"));
		xmlnode_hide(xmlnode_get_tag(q,"last"));
		xmlnode_hide(xmlnode_get_tag(q,"email"));

		/* don't send passwd */
		xmlnode_hide(xmlnode_get_tag(q,"password"));
		xmlnode_insert_tag(q,"password");

		/* remove key */
		while ((x = xmlnode_get_tag(q,"key")) != NULL) xmlnode_hide(x);

		/* add key */
		key = jutil_regkey(NULL,jid_full(jp->from));
		xmlnode_insert_cdata(xmlnode_insert_tag(q,"key"),key,-1);

		/* add instructions */
		xmlnode_insert_cdata(xmlnode_insert_tag(q,"instructions"),ti->registration_instructions,-1);

		/* mark we are registered */
		xmlnode_insert_tag(q,"registered");
		
		/* create and fill in jabber:x:data form */
		if (!ti->no_x_data) {
		  xmlnode qq;
		  qq = xdata_create(q, "form");
		  xmlnode_insert_cdata(xmlnode_insert_tag(qq,"title"),"Registration to ICQ by JIT",-1);
		  xmlnode_insert_cdata(xmlnode_insert_tag(qq,"instructions"),ti->registration_instructions,-1);
	  
		  xdata_insert_field(qq,"text-single","username","UIN",xmlnode_get_tag_data(q,"username"));
		  xdata_insert_field(qq,"text-private","password","Password",NULL);
		  xdata_insert_field(qq,"hidden","key",NULL,key);
		  xdata_insert_field(qq,"hidden","registered",NULL,NULL);
		}
	  }
	  else {
		/* not registered */
        xmlnode_insert_tag(q,"username");
        xmlnode_insert_tag(q,"password");
		
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"instructions"),ti->registration_instructions,-1);
		
		key = jutil_regkey(NULL,jid_full(jp->from));
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"key"),key,-1);
		
		/* create and fill in jabber:x:data form */
		if (!ti->no_x_data) {
		  q = xdata_create(q, "form");
		  xmlnode_insert_cdata(xmlnode_insert_tag(q,"title"),"Registration to ICQ by JIT",-1);
		  xmlnode_insert_cdata(xmlnode_insert_tag(q,"instructions"),ti->registration_instructions,-1);
		  
		  xdata_insert_field(q,"text-single","username","UIN",NULL);
		  xdata_insert_field(q,"text-private","password","Password",NULL);
		  xdata_insert_field(q,"hidden","key",NULL,key);
		}
	  }
	}
	else
	  jutil_error(jp->x,TERROR_NOTALLOWED);
	
    it_deliver(ti,jp->x);
}

void it_unknown_reg_set(iti ti, jpacket jp)
{
    session s;
    xmlnode q = jp->iq;
    UIN_t uin;
    queue_elem queue;
    char *user, *pass;
    int xdata;

    if (ti->registration_instructions == NULL)
    {
        jutil_error(jp->x,TERROR_NOTALLOWED);
        it_deliver(ti,jp->x);
        return;
    }

    xdata = xdata_test(q,"submit");
    
    if (xdata)
    {
	  pass = xdata_get_data(q,"password");
	  user = xdata_get_data(q,"username");
    }
    else
    {
	  pass = xmlnode_get_tag_data(q,"password");
	  user = xmlnode_get_tag_data(q,"username");
    }

    if(!user || !pass)
    {
        jutil_error(jp->x,TERROR_NOTACCEPTABLE);
        it_deliver(ti,jp->x);
        return;
    }

    uin = it_strtouin(user);
    if (uin == 0)
    {
        jutil_error(jp->x,TERROR_NOTACCEPTABLE);
        it_deliver(ti,jp->x);
        return;
    }

    s = it_session_create(ti,jp);
    if (s==NULL) {
	  s = (session) wpxhash_get(ti->sessions,jid_full(jid_user(jp->from)));
	  SEM_UNLOCK(ti->sessions_sem);

	  if (s==NULL) {
		log_alert(ZONE,"failed to create session");
		xmlnode_free(jp->x);
		return;
	  }
	  log_debug(ZONE,"Session %s already created",jid_full(jp->from));
	  jp->aux1 = (void *) s;
	  mtq_send(s->q,jp->p,it_session_jpacket,(void *) jp);
	  return;
    }
    s->type = stype_register;
    s->uin = uin;
    
    if (s->uin) {
      session_ref s_alt;
      char buffer[16];
      s_alt=pmalloco(s->p,sizeof(_session_ref));
      s_alt->s=s;
      snprintf(buffer,16,"%lu",s->uin);
      wpxhash_put(ti->sessions_alt,pstrdup(s->p,buffer),(void*)s_alt);
    };
    
    s->passwd = it_convert_utf82windows(s->p,pass);
    if (strlen(s->passwd)>8) s->passwd[8]='\0';

    /* add to queue */
    queue = pmalloco(jp->p,sizeof(_queue_elem));
    queue->elem = (void *)jp;
    /* next = NULL */
    QUEUE_PUT(s->queue,s->queue_last,queue);

    StartClient(s);

	SEM_UNLOCK(ti->sessions_sem);
}

void it_unknown_bounce(void *arg)
{
    jpacket jp = (jpacket) arg;
    iti ti = (iti) jp->aux1;
    xmlnode reg;

    reg = xdb_get(ti->xc,it_xdb_id(jp->p,jp->from,jp->to->server),NS_REGISTER);
    if (reg != NULL)
    {
        jutil_error(jp->x,(terror){404,"Session Not Found"});
        xmlnode_free(reg);
    }
    else
        jutil_error(jp->x,TERROR_REGISTER);

    it_deliver(ti,jp->x);
}
