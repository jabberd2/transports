/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 *  Contact list management */

#include "icqtransport.h"

#define CONTACT_OFFLINE(c) (c->status == ICQ_STATUS_OFFLINE || c->status == ICQ_STATUS_NOT_IN_LIST) 

/** Load contact roster.
 * Read the Jabber user's main Jabber roster and insert all contacts
 * on this transport to our session's contact list.
 * This will fail if the user connects from a different server
 * than this transport is running on. */
void it_contact_load_roster(session s)
{
  xmlnode roster, item;
  UIN_t uin;
  contact c;

  s->roster_changed = 0;

  if (s->exit_flag)
    return;

  /* get own roster */
  if (s->ti->own_roster) {
    roster = xdb_get(s->ti->xc,
					 it_xdb_id(s->p,s->id,s->from->server)
					 ,NS_ROSTER);
	if (roster != NULL) {

	  log_debug("roster","found user roster");
	  
	  /* loop roster */
	  for (item = xmlnode_get_firstchild(roster); item != NULL; item = xmlnode_get_nextsibling(item))   {
		UIN_t uin;

		if (xmlnode_get_type(item) != NTYPE_TAG) continue;
		if (xmlnode_get_attrib(item,"jid") == NULL)  continue;

		uin = it_strtouin(xmlnode_get_attrib(item,"jid"));

		log_debug("OWN_FOUND","Contact %d",uin);

        if (uin == SMS_CONTACT)
          c = it_sms_get(s,xmlnode_get_attrib(item,"jid"));
        else
          c = it_contact_get(s,uin);

		if (!c) {
		  if (uin == SMS_CONTACT) {
			log_debug("SMS","ADD %s",xmlnode_get_attrib(item,"jid"));
			c = it_sms_add(s,xmlnode_get_attrib(item,"jid"));
			c->status = ICQ_STATUS_OFFLINE;
			s->contact_count++;
		  }
		  else 
			if (uin && uin != s->uin)  {
			  c = it_contact_add(s,uin);
			  log_debug(ZONE,"Contact ADD %d",uin);
			  // propagate contact to C++ backend
			  AddICQContact(c);
			  
			  c->status = ICQ_STATUS_OFFLINE;
			  s->contact_count++;			
			}
		}
		
	  }
	}
	xmlnode_free(roster);
  }

  s->roster_changed = 0;
  
  if (s->ti->no_jabber_roster) 
	return;
  
  roster = xdb_get(s->ti->xc,s->id,NS_ROSTER); 
  if (roster == NULL)
    return;

  for (item = xmlnode_get_firstchild(roster); item != NULL; item = xmlnode_get_nextsibling(item))   {
	jid tmpjid;

    if (xmlnode_get_type(item) != NTYPE_TAG) continue;
    if (xmlnode_get_attrib(item,"jid") == NULL)  continue;
    if (xmlnode_get_attrib(item,"subscribe") != NULL) continue;

	tmpjid = jid_new(xmlnode_pool(roster),xmlnode_get_attrib(item,"jid"));
        
    log_debug(ZONE,"contact %s",tmpjid->server);

    /* SMS contact */
    if (j_strcmp(s->ti->sms_id,tmpjid->server)==0) {
      log_debug(ZONE,"SMS contact ADD %s",tmpjid->user);
      c = it_sms_add(s,tmpjid->user);
      c->status = ICQ_STATUS_OFFLINE;
      s->contact_count++;
      continue;
    }
    
    /* ignore transport itself */
    if (jid_cmpx(s->from,tmpjid,JID_SERVER)) continue;
    
    uin = it_strtouin(tmpjid->user);
    if (uin && uin != s->uin && it_contact_get(s,uin) == NULL)  {
	  c = it_contact_add(s,uin);
      log_debug(ZONE,"Contact ADD %d",uin);
      // propagate contact to C++ backend
      AddICQContact(c);
	  
      c->status = ICQ_STATUS_OFFLINE;
      s->contact_count++;
      
      //      if (xmlnode_get_attrib(item,"ask")) 
      //        c->asking = 1; 
    }
  }

  s->roster_changed = 0;
  
  xmlnode_free(roster);
}

void it_save_contacts(session s) {
  log_debug(ZONE,"try to save contacts");

  if (s->ti->own_roster) {
	xmlnode roster;
	contact c;
	int data = 0;
	char buf[30];
	
	roster = xmlnode_new_tag("query");
	xmlnode_put_attrib(roster,"xmlns",NS_ROSTER);
	
	c = s->contacts; 
	log_debug(ZONE,"save contacts");
	
	while (c) { 
	  if (c->status != ICQ_STATUS_NOT_IN_LIST) {
		data = 1;
		
		if ((c->uin == SMS_CONTACT)&&(c->sms)) {
		  snprintf(buf,25,"%s",c->sms);
		}
		else {
		  snprintf(buf,15,"%lu",c->uin);
		}
		
		xmlnode_put_attrib(xmlnode_insert_tag(roster,"item"),"jid",buf);
		log_debug(ZONE,"save contact %s",buf);
	  }
	  c = c->next; 
	} 
	
	if (data) {
	  if (xdb_set(s->ti->xc,it_xdb_id(s->p,s->id,s->from->server),NS_ROSTER,roster)) {
		log_debug(ZONE,"Error saving contacts");
		xmlnode_free(roster);
	  }
	}
	else {
	  log_debug(ZONE,"Nothing to save");
	  xmlnode_free(roster);
	}
  }
}

/** Add unknown users
  */
contact it_unknown_contact_add(session s, char *user, UIN_t uin) {
  contact c = NULL;

  if (uin == SMS_CONTACT) {
	c = it_sms_add(s,user);
	log_debug(ZONE,"sms add %s",user);
	log_debug(ZONE,"subscribe");
	it_contact_subscribe(c,NULL);

	if ((s->ti->own_roster)&&(s->connected)) {
	  it_save_contacts(s);
	}
  }
  else
	if ((uin > 0) && (uin != s->uin)) {
	  c = it_contact_add(s,uin);
	  log_debug(ZONE,"contact add");
	  log_debug(ZONE,"subscribe");
	  it_contact_subscribe(c,NULL);

	  
	  if ((s->ti->own_roster)&&(s->connected)) {
		it_save_contacts(s);
	  }
	}

  return c;
}


/** Walk through all SMS contacts and set status to
 * 0 - offline, 1 - online */
void it_sms_presence(session s,int status) 
{ 
     contact c; 
     c = s->contacts; 

     while (c) 
     { 
       if (c->uin == SMS_CONTACT) {
         if (status)
           it_contact_set_status(c, s->ti->sms_show,s->ti->sms_status);
         else
           it_contact_set_status(c, ICQ_STATUS_OFFLINE,NULL);
       }
       c = c->next; 
     } 

     log_debug(ZONE,"sms contacts pres %d",status);
}

/** Search UIN in contact list
 * @param uin UIN to search for
 * @return Contact or NULL if not found */
contact it_contact_get(session s, UIN_t uin)
{
    contact c;

    for (c = s->contacts; c != NULL; c = c->next)
        if (c->uin == uin) break;

    return c;
}

/** Search SMS contact
 * @param id SMS contact to search for
 * @return Contact or NULL if not found */
contact it_sms_get(session s, char * id)
{
    contact c;

    for (c = s->contacts; c != NULL; c = c->next)
        if (c->uin == SMS_CONTACT) {
      if (j_strcmp(c->sms,id)==0)
        break;
    }

    return c;
}

/** Insert contact in session's contact list */
contact it_contact_add(session s, UIN_t uin)
{
    contact c;
    pool p;

    p = pool_heap(128);
    c = (contact) pmalloco(p,sizeof(_contact));
    c->p = p;

    c->uin = uin;
    c->status = ICQ_STATUS_NOT_IN_LIST;
    c->s = s;
    c->use_chat_msg_type = s->ti->msg_chat;

    // insert new contact in linear list 
    c->next = s->contacts;
    s->contacts = c;
	
	s->roster_changed = 1;

    return c;
}

/** Add SMS contact to session's contact list
 * don't give NULL id */
contact it_sms_add(session s, char * id)
{
    contact c;
    pool p;

    p = pool_heap(256);
    c = (contact) pmalloco(p,sizeof(_contact));
    c->p = p;

    c->uin = SMS_CONTACT;
    c->sms = pstrdup(p,id);
    c->status = ICQ_STATUS_OFFLINE;
    c->s = s;
    
    // insert new contact in linear list 
    c->next = s->contacts;
    s->contacts = c;
	
	s->roster_changed = 1;

    return c;
}

/** Remove contact from session's contact list */
void it_contact_remove(contact c)
{
    session s = c->s;
    contact cur, prev;

    for (prev = NULL, cur = s->contacts; cur != c; prev = cur, cur = cur->next);

    // update linear list
    if (prev)
        prev->next = cur->next;
    else
        s->contacts = cur->next;

    pool_free(c->p);

	s->roster_changed = 1;

	if ((s->ti->own_roster)&&(s->connected)) {
	  it_save_contacts(s);
	}
}

/** Set contact's status */
void it_contact_set_status(contact c, icqstatus show, char * status)
{ 
  if ((c->status != show)||(status != NULL)) {
    c->status = show; 
    it_contact_send_presence(c,status); 
  }
} 

/** Send contact's status to Jabber side */
void it_contact_send_presence(contact c,char * status) 
{ 
  session s = c->s; 
  xmlnode pres, x; 
  jid id; 
  
  pres = jutil_presnew(CONTACT_OFFLINE(c) ? JPACKET__UNAVAILABLE : JPACKET__AVAILABLE,jid_full(s->id),NULL);

  if(status != NULL)
    xmlnode_insert_cdata(xmlnode_insert_tag(pres,"status"),
                         it_convert_windows2utf8(xmlnode_pool(pres),status),
                         -1);
  
  if (c->uin == SMS_CONTACT) 
    id = it_sms2jid(xmlnode_pool(pres),c->sms,s->ti->sms_id); 
  else
    id = it_uin2jid(xmlnode_pool(pres),c->uin,s->from->server); 

  xmlnode_put_attrib(pres,"from",jid_full(id));

  switch (c->status) { 
     case ICQ_STATUS_AWAY: 
       x = xmlnode_insert_tag(pres,"show"); 
       xmlnode_insert_cdata(x,"away",-1); 
       break; 
     case ICQ_STATUS_DND:
       x = xmlnode_insert_tag(pres,"show");
       xmlnode_insert_cdata(x,"dnd",-1);
       break;
     case ICQ_STATUS_NA:
     case ICQ_STATUS_OCCUPIED:
       x = xmlnode_insert_tag(pres,"show");
       xmlnode_insert_cdata(x,"xa",-1);
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

/** Add contact to roster */
void it_contact_subscribe(contact c,const char * name)
{
  xmlnode x;
  session s = c->s;

  if (c->uin == SMS_CONTACT) {
    /* This is a SMS contact, simply add to roster. */
    c->status = s->ti->sms_show;
    s->contact_count++;
	
    /* change jabber main roster */
    x = jutil_presnew(JPACKET__SUBSCRIBED,jid_full(s->id),NULL);
    xmlnode_put_attrib(x,"from",jid_full(it_sms2jid(xmlnode_pool(x),
                            c->sms,s->ti->sms_id)));

    it_deliver(s->ti,x);

    it_contact_set_status(c, s->ti->sms_show,s->ti->sms_status);

    return;
  }

  /* This is a standard contact */
  c->status = ICQ_STATUS_OFFLINE;
  s->contact_count++;
    
  /* Propagate to C++ backend */
  AddICQContact(c);

  /* We do not want to request auth on auto-import of contacts */
  if(name == NULL) {
    /* Request auth from ICQ side
       We don't need that for presence but this is the only way to contact
       peers who ignore messages from contacts not on their contact list */
    SendAuthRequest(c,LNG_AUTH_REQUEST);
  }

  /* Send "subscribed" presence. Fortunately, jabberd will add this
     contact to the user's roster if it was not present */
  x = jutil_presnew(JPACKET__SUBSCRIBED,jid_full(s->id),NULL);
  xmlnode_put_attrib(x,"from",jid_full(it_uin2jid(xmlnode_pool(x),c->uin,s->from->server)));
  if(name != NULL) xmlnode_put_attrib(x,"name",name);
  it_deliver(s->ti,x);
}

/** Remove contact from Jabber roster and session's contact list */
void it_contact_unsubscribe(contact c)
{
    session s = c->s;
    iti ti = s->ti;
    xmlnode x;

    log_debug(ZONE,"unsubscribe");

    if (c->uin == SMS_CONTACT) {
      /* This is a SMS contact, remove from roster. */
      x = jutil_presnew(JPACKET__UNSUBSCRIBED,jid_full(s->id),NULL);
      xmlnode_put_attrib(x,"from",jid_full(it_sms2jid(xmlnode_pool(x),c->sms,
                              s->ti->sms_id)));

      /* remove from session's contact list */
      c->status = ICQ_STATUS_NOT_IN_LIST;
      s->contact_count--;
      it_contact_remove(c);
      
      /* remove from roster */
      it_deliver(ti,x);
      return;
    }

    /* This is standard contact, remove from roster. */
    x = jutil_presnew(JPACKET__UNSUBSCRIBED,jid_full(s->id),NULL);
    xmlnode_put_attrib(x,"from",jid_full(it_uin2jid(xmlnode_pool(x),c->uin,s->from->server)));

    /* propagate change to C++ backend */
    SendRemoveContact(c);

    /* remove from session's contact list */
    c->status = ICQ_STATUS_NOT_IN_LIST;
    s->contact_count--;
    it_contact_remove(c);
    it_deliver(ti,x);
}

/** Jabber side authorized a contact's request */
void it_contact_subscribed(contact c, jpacket jp) {
  if (c->uin == SMS_CONTACT) return;

  /* propagate to C++ backend */
  AddICQContact(c);
  
  /* send auth given */
  SendAuthGiven(c);

  /* send it */
  SendAuthRequest(c,LNG_AUTH_REQUEST);
}

/** Jabber side denied authorization */
void it_contact_unsubscribed(contact c, jpacket jp) {
  if (c->uin == SMS_CONTACT) return;

  /** propagate to C++ backend */
  /* XXX should have asking flag */
  SendAuthDenied(c);

  if (c->status != ICQ_STATUS_NOT_IN_LIST) {
    SendRemoveContact(c);
  }

  it_contact_remove(c);
}

/** Destroy session's contact list */
void it_contact_free(session s) 
{ 
     contact c; 
     pool p; 
     c = s->contacts; 
     log_debug(ZONE,"free contacts");
     while (c) 
     { 
       p = c->p; 
       /* our Client class informs other contacts that we are dead */
       c = c->next; 
       pool_free(p); 
     } 
     s->contacts = NULL;
}






