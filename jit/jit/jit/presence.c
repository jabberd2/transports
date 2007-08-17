/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 *  Presence and subscription (s10n) handling */

#include "icqtransport.h"

void it_contact_unsubscribed(contact c,jpacket jp);

/** Process presence from Jabber */
void it_presence(session s, jpacket jp)
{
    log_debug(ZONE,"Session[%s], handling presence",jid_full(s->id));

    switch (jpacket_subtype(jp))
    {
    case JPACKET__PROBE:
      if (jp->to->user) {
        UIN_t uin;
        contact c;
        uin = it_jid2uin(jp->to);
      
		if (j_strcmp(jp->to->server,s->ti->sms_id)==0) {
		  uin = SMS_CONTACT;
		}

        if (uin == SMS_CONTACT)
          c = it_sms_get(s,jp->to->user);
        else
          c = it_contact_get(s,uin);

        if (c != NULL)
          it_contact_send_presence(c,NULL);
		else {
		  c = it_unknown_contact_add(s, jp->to->user, uin);
		  if (c != NULL)
			it_contact_send_presence(c,NULL);
		}
      }
      break;

    case JPACKET__INVISIBLE:
      {
      icqstatus status;
      char *text;

      if (jp->to->user) /* presence to groupchat here */
        break;

      /* update users presence */
      s->p_db = ppdb_insert(s->p_db,jp->from,jp->x);

      text = xmlnode_get_tag_data(jp->x,"status");
      
      if (text != NULL)
        strncpy(s->status_text,text,MAX_STATUS_TEXT);
      else
        s->status_text[0] = '\0';
      
      status = ICQ_STATUS_INVISIBLE;
      if (status != s->status) {
        s->status = status;
        /* propagate to C++ backend */
        SendStatus(s);
      }
      
      /* echo.... */
      /* only when connected */
      if (!s->connected) {
        xmlnode_free(jp->x);
        return;
      }
      
      xmlnode_put_attrib(jp->x,"from",jid_full(jp->to));
      xmlnode_put_attrib(jp->x,"to",jid_full(jid_user(jp->from)));
      it_deliver(s->ti,jp->x);
      return;
      }

    case JPACKET__AVAILABLE:
      {
        icqstatus status;
        char *text;

        if (jp->to->user) /* presence to groupchat here */
            break;

        log_debug(ZONE,"presence");

        /* update user's presence */
        s->p_db = ppdb_insert(s->p_db,jp->from,jp->x);

        text = xmlnode_get_tag_data(jp->x,"status");

        if (text != NULL) {
          strncpy(s->status_text,text,MAX_STATUS_TEXT);
        }
        else
          s->status_text[0] = '\0';
    
        status = jit_show2status(xmlnode_get_tag_data(jp->x,"show"));
        if (status != s->status) {
          s->status = status;
          /* Always set status regardless of being connected or not */
          /* Client class will set this status */
          /* after connecting */
          SendStatus(s);
        }

        /* echo.... */
        /* only when connected */
        if (!s->connected) {
          xmlnode_free(jp->x);
          return;
        }
    
        xmlnode_put_attrib(jp->x,"from",jid_full(jp->to));
        xmlnode_put_attrib(jp->x,"to",jid_full(jid_user(jp->from)));
        it_deliver(s->ti,jp->x);
        return;
    }

    case JPACKET__UNAVAILABLE:
        if (jp->to->user == NULL)
        { /* kill the session if there are no available resources left */
          s->p_db = ppdb_insert(s->p_db,jp->from,jp->x);
          if (ppdb_primary(s->p_db,s->id) == NULL){
            if (!s->exit_flag)
              EndClient(s); // will call it_session_end
          }
        }

    default:
        break;
    }

    xmlnode_free(jp->x);
}

/** Got a subscription request from Jabber */
void it_s10n(session s, jpacket jp)
{
  UIN_t uin;
  contact c;

  if (jp->to->user == NULL) {
    /* ignore s10n to the transport */
    xmlnode_free(jp->x);
    return;
  }

  uin = it_jid2uin(jp->to);
  if (uin == 0 || uin == s->uin) {
    jutil_error(jp->x,TERROR_BAD);
    it_deliver(s->ti,jp->x);
    return;
  }

  if (s->connected == 0) {
    queue_elem queue;

    /* add to circle queue */
    queue = pmalloco(jp->p,sizeof(_queue_elem));
    queue->elem = (void *)jp;
    /* next = NULL */

    QUEUE_PUT(s->queue,s->queue_last,queue);
    return;
  }

  /* we're connected, go on */
  log_debug(ZONE,"presence packet uin = %d",uin);
  
  /* check for SMS_CONTACT */
  if (j_strcmp(jp->to->server,s->ti->sms_id)==0) {
	uin = SMS_CONTACT;
  }
  
  if (uin == SMS_CONTACT)
    c = it_sms_get(s,jp->to->user);
  else
    c = it_contact_get(s,uin);
  
  switch (jpacket_subtype(jp))
      {
      case JPACKET__SUBSCRIBE:
        if (c == NULL) {
          /* if sms contact */
          if (uin == SMS_CONTACT) {
            /* if not our sms id */
            if (j_strcmp(jp->to->server,s->ti->sms_id)) {
              log_debug(ZONE,"not our sms %s",jp->to->server);
              xmlnode_free(jp->x);
              break;
            }
            c = it_sms_add(s,jp->to->user);
            log_debug(ZONE,"sms add");
          }
          else     
            c = it_contact_add(s,uin);
        }

        log_debug(ZONE,"subscribe");

		it_contact_subscribe(c,NULL);

        xmlnode_free(jp->x);
        break;
    
      case JPACKET__SUBSCRIBED:
        /* after asking */
        if (c) {
          it_contact_subscribed(c,jp);      
          log_debug(ZONE,"subscribed");
        }
        
        xmlnode_free(jp->x);
        break;
        
      case JPACKET__UNSUBSCRIBE:    
        if (c) {
          /* will remove user from icq contacts 
             inform main jabber roster unsubscribed */
          it_contact_unsubscribe(c);
          log_debug(ZONE,"unsubscribe");      
        }
        
        xmlnode_free(jp->x);
        break; 
        
      case JPACKET__UNSUBSCRIBED:
        /* when icq ask for subscribe we have that contacts in our roster 
           remove contact if exist */
        if (c) {      
          it_contact_unsubscribed(c,jp);
          log_debug(ZONE,"unsubscribed");
        }
        
        xmlnode_free(jp->x);
        break;
        
      default:
        xmlnode_free(jp->x);
        break;
      }
}




