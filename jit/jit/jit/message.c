/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 *  Handling of outgoing (to ICQ) messages */

#include "icqtransport.h"

/** Got message from Jabber. Propagate to C++ backend. */
void it_message(session s, jpacket jp)
{
    UIN_t uin;
	contact c;
    xmlnode x;

    uin = it_jid2uin(jp->to);
    if (uin == 0 || s->uin == uin)
    {
        jutil_error(jp->x,TERROR_BAD);
        it_deliver(s->ti,jp->x);
        return;
    }

	if ((jp->to == NULL) || (jp->to->user == NULL)) {
      jutil_error(jp->x,TERROR_NOTIMPL);
      it_deliver(s->ti,jp->x);
	  return;
	}

    if (s->connected == 0)
    {
      /* Not yet connected, enqueue packet */
      queue_elem queue;

      queue = pmalloco(jp->p,sizeof(_queue_elem));
      queue->elem = (void *)jp;
      /* next = NULL */

      QUEUE_PUT(s->queue,s->queue_last,queue);
      return;
    }

	if (j_strcmp(jp->to->server,s->ti->sms_id)==0) {
	  uin = SMS_CONTACT;
	}
	
	if (uin == SMS_CONTACT)
	  c = it_sms_get(s,jp->to->user);
	else
	  c = it_contact_get(s,uin);
	
	//	if (c == NULL) {
	//	  c = it_unknown_contact_add(s,jp->to->user,uin);
	//	}

    if (j_strcmp(jp->to->server,s->ti->sms_id)==0) {
      /* Send SMS message */
      char* body= xmlnode_get_tag_data(jp->x,"body");
      
      log_debug(ZONE,"SMS Message process");

      if(body == NULL || strlen(body)==0) {
        jutil_error(jp->x,
                    (terror){400,LNG_EMPTY_MESSAGE});
        it_deliver(s->ti,jp->x);
        return;
      }
      
      if(strlen(body)>=160){
        jutil_error(jp->x,
                    (terror){400,LNG_LONG_MESSAGE});
        it_deliver(s->ti,jp->x);
        return;
      }
      
      /* propagate to C++ backend */
      SendSMS(s,body,jp->to->user);
      
      xmlnode_free(jp->x);
      return;
    }

    /* Send non-SMS message */
    log_debug(ZONE,"Message process");
  
    if ((x = xmlnode_get_tag(jp->x,"x?xmlns=jabber:x:roster"))) {
      jutil_error(jp->x,TERROR_NOTIMPL);
      it_deliver(s->ti,jp->x);
      return;
    }
    else 
      if ((x = xmlnode_get_tag(jp->x,"x?xmlns=jabber:x:oob")) != NULL) {
        /* Send URL */
        char * u, *d,*url,*desc;
        url=xmlnode_get_tag_data(x,"url");
        desc=xmlnode_get_tag_data(x,"desc");
       
        if(url==NULL) {
          jutil_error(jp->x,
                      (terror){400,"Empty urls are forbidden"});
          it_deliver(s->ti,jp->x);
          return;
        }
       
        u = it_convert_utf82windows(jp->p,url);
        d = it_convert_utf82windows(jp->p,desc?desc:"");
       
        if (u==NULL || d==NULL) {
          jutil_error(jp->x,
                      (terror){400,"Bad UTF8"});
          it_deliver(s->ti,jp->x);
          return;
        }
       
        if((d&&!strlen(d)) || !strlen(u)){
          jutil_error(jp->x,
                      (terror){400,"Bad UTF8"});
          it_deliver(s->ti,jp->x);
          return;
        }

        /* propagate to C++ backend */
        SendUrl(s,u,d,uin);
       
        xmlnode_free(jp->x);
        return;
      }
      else {
        /* standard message */
        char* b;
        char* body= xmlnode_get_tag_data(jp->x,"body");
        unsigned char chat;
       
        if(body == NULL) {
          jutil_error(jp->x,
                      (terror){400,LNG_EMPTY_MESSAGE});
          it_deliver(s->ti,jp->x);
          return;
        }
       
        b = it_convert_utf82windows(jp->p,body);
        if (b==NULL) {
          jutil_error(jp->x,
                      (terror){400,"Bad UTF8"});
          it_deliver(s->ti,jp->x);
          return;
        }
       
        if (xmlnode_get_attrib(jp->x,"type")&&
            j_strcmp(xmlnode_get_attrib(jp->x,"type"),"chat")==0)
          chat = 1;
        else
          chat = 0;

        /* remember chat */    
        if (c != NULL) {
          c->use_chat_msg_type = chat;
        }
       
        /* propagate to C++ backend */
        SendMessage(s,b,chat,uin);
       
        xmlnode_free(jp->x);
      }
}
