/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Written by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 * libicq2000 JIT client class */

#include "icqtransport.h"

#include "wp_client.h"

#ifndef ZONE
#define ZONE "wp_client.cpp"
#endif

#include <time.h>

using std::ostringstream;
using std::endl;

extern "C" void it_server_auth(mio m, int state, void *arg, char *buffer, int len);
extern "C" void it_server_bos(mio m, int state, void *arg, char *buffer, int len);

WPclient::WPclient(unsigned int uin,const string& passwd) 
  : Client(uin,passwd) {
  WPInit();
}

void WPclient::WPInit() {
  sesja = NULL;
  search_ev = NULL;
}

WPclient::~WPclient() {
  /* send presences and disconnect */
  Disconnect(DisconnectedEvent::REQUESTED);
  sesja = NULL;  
}

void WPclient::SetSession(session arg) {
  sesja = arg;  
}


void WPclient::SetStatus() {
  Status st;
  bool inv = false;

  switch (sesja->status)
    {
    case ICQ_STATUS_INVISIBLE:
      st = STATUS_ONLINE;
      inv = true;
    case ICQ_STATUS_ONLINE:
      st = STATUS_ONLINE;
      break;
    case ICQ_STATUS_AWAY:
      st = STATUS_AWAY;
      break;
    case ICQ_STATUS_DND:
      st = STATUS_DND;
      break;
    case ICQ_STATUS_NA:
      st = STATUS_NA;
      break;
    case ICQ_STATUS_OCCUPIED:
      st = STATUS_OCCUPIED;
      break;
    case ICQ_STATUS_FREE_CHAT:
      st = STATUS_FREEFORCHAT;
      break;
    default:
      st = STATUS_ONLINE;
      break;
    }

  log_debug(ZONE,"Set status %d,%d",st,inv);

  this->setStatus(st,inv);
}

/* RECIVED RECIVED RECIVED RECIVED RECIVED */
/* send status  */
void WPclient::sendContactPresence(const unsigned int uin, const string& status){
  contact jc;
  ContactRef c=getContact(uin);

  if(!c.get())
    return;
    
  /* if not our contact, fu.. him */
  jc = it_contact_get(sesja,uin);
  if(jc==NULL) {
    log_alert(ZONE,"contact in icq, but not in roster");
    return;
  }

  Status st = c->getStatus();

  //  if (STATUS_FLAG_INVISIBLE & st) {
  //    it_contact_set_status(jc,ICQ_STATUS_INVISIBLE,
  //              NULL);
  //    return;
  //  }

  switch (st)
    {
    case STATUS_OFFLINE:
      it_contact_set_status(jc,ICQ_STATUS_OFFLINE,
                NULL);
      break;

    case STATUS_AWAY:
      it_contact_set_status(jc,ICQ_STATUS_AWAY,
                status.size()==0?NULL:(char *)status.c_str());
      break;
    case STATUS_DND:
      it_contact_set_status(jc,ICQ_STATUS_DND,
                status.size()==0?NULL:(char *)status.c_str());
      break;
    case STATUS_NA:
      it_contact_set_status(jc,ICQ_STATUS_NA,
                status.size()==0?NULL:(char *)status.c_str());
      break;
    case STATUS_OCCUPIED:
      it_contact_set_status(jc,ICQ_STATUS_OCCUPIED,
                status.size()==0?NULL:(char *)status.c_str());
      break;
    case STATUS_FREEFORCHAT:
      it_contact_set_status(jc,ICQ_STATUS_FREE_CHAT,
                status.size()==0?NULL:(char *)status.c_str());
      break;
    default:
      it_contact_set_status(jc,ICQ_STATUS_ONLINE,
                status.size()==0?NULL:(char *)status.c_str());
      break;
    }
}

/* external from Client */
void WPclient::SignalConnected(ConnectedEvent *ev) {
  log_debug(ZONE,"Connected!!");

  it_session_confirmed(sesja);    
}

void WPclient::SignalDisconnected(DisconnectedEvent *ev) {
  terror e;
  
  switch(ev->getReason()){
  case DisconnectedEvent::FAILED_BADUSERNAME:
    e = (terror){400,"Bad username"};
    break;

  case DisconnectedEvent::FAILED_TURBOING:
    e = (terror){503,"Turboing, connect later"};
    break;

  case DisconnectedEvent::FAILED_BADPASSWORD:
    e = (terror){400,"Bad (non mismatched) registration password"};
    break;

  case DisconnectedEvent::FAILED_MISMATCH_PASSWD:
    e = (terror){401,"Password does not match"};
    break;

  case DisconnectedEvent::FAILED_DUALLOGIN: {
    /* maybe we should explicitly notify due importance */
    char *body=LNG_DUAL_LOGIN;

    xmlnode msg = xmlnode_new_tag("message");
    xmlnode_put_attrib (msg, "to", jid_full(sesja->id));
    xmlnode_insert_cdata (xmlnode_insert_tag (msg, "body"), 
						  it_convert_windows2utf8(xmlnode_pool(msg),body),
						  (unsigned int)-1);
    xmlnode_put_attrib(msg,"from",jid_full(sesja->from));
    it_deliver(sesja->ti,msg);

    e = (terror){409,"Dual login"};
    break;
  }

  case DisconnectedEvent::FAILED_LOWLEVEL:
    e = (terror){502,"Low level network error"};
    break;

  case DisconnectedEvent::REQUESTED:
    log_debug(ZONE,"Disconnected on request");
    e = (terror){0,""};
    break;    

  case DisconnectedEvent::FAILED_UNKNOWN:
  default:
    e = (terror){502,"Disconnected by unknown reason"};
    break;
  }
  
  if (e.code != 0)
    it_session_error(sesja,e);
  else 
    it_session_end(sesja); /* on request for BOS */
}

void WPclient::SignalMessaged(MessageEvent *ev) {
    jid id;
    xmlnode msg;
	unsigned int uin = 0;
    ContactRef c = ev->getContact();

	if (c->isVirtualContact())
	  uin = 0;
	else
	  uin = c->getUIN();
	
    switch(ev->getType()){
    case MessageEvent::Normal: {
        NormalMessageEvent* nm=static_cast<NormalMessageEvent*>(ev);
        msg = xmlnode_new_tag("message");
        contact sc;
        bool type_chat;

        sc = it_contact_get(sesja,uin);
        if (sc)
          type_chat = sc->use_chat_msg_type;
        else
          if (sesja->ti->msg_chat)
            type_chat = true;
          else
            type_chat = false;
        
        if(type_chat)
          xmlnode_put_attrib(msg,"type","chat");
        
        log_debug(ZONE,"Message: len->%i encoding->%i text->%s",int((nm->getMessage()).length()),
                                                              int(nm->getTextEncoding()),(nm->getMessage()).c_str());
        if (nm->getTextEncoding()==2)      // UCS2BE
          xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
                             it_convert_ucs2utf8(xmlnode_pool(msg),
                                                 (nm->getMessage()).length(),
                                                 (nm->getMessage()).c_str()),
                             (unsigned int)-1);
        else if (nm->getTextEncoding()==8) // UTF-8
          xmlnode_insert_cdata( xmlnode_insert_tag(msg,"body") ,
                             (nm->getMessage()).c_str()     ,
                             (unsigned int)-1);
        
        else                        // 0==ASCII and 3==locale_encoded will go here.
          xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
                             it_convert_windows2utf8(xmlnode_pool(msg),
                                                     (nm->getMessage()).c_str()),
                             (unsigned int)-1);

	if ((ev->getTime()!=0) && ((ev->getTime()+120)<time(NULL))) {
                // if the message travels more than 2 minutes it is not online anyway. Stamp it.
    		xmlnode x;
    		char timestamp[24];
		const time_t time=ev->getTime();
		struct tm *t=gmtime(&time);

		snprintf(timestamp,24,"%04i%02i%02iT%02i:%02i:%02i",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
		x=xmlnode_insert_tag(msg,"x");
		xmlnode_put_attrib(x,"xmlns","jabber:x:delay");
		xmlnode_put_attrib(x,"stamp",timestamp);
        }
        break;
    }
    case MessageEvent::URL: {
        URLMessageEvent* urlm=static_cast<URLMessageEvent*>(ev);
        msg = xmlnode_new_tag("message");

        xmlnode_insert_cdata(xmlnode_insert_tag(msg,"subject"),
                     "ICQ URL Message",(unsigned int)-1);

        /* add a message body that most clients can understand */
        xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
                     it_convert_windows2utf8(xmlnode_pool(msg),
                                 ("URL: "+(urlm->getURL())+"\nDescription:\n"+
                                  (urlm->getMessage())).c_str()),
                     (unsigned int)-1);

        xmlnode x = xmlnode_insert_tag(msg,"x");
        xmlnode_put_attrib(x,"xmlns",NS_XOOB);
        xmlnode_insert_cdata(xmlnode_insert_tag(x,"desc"),
                     it_convert_windows2utf8(xmlnode_pool(msg),
                                 (urlm->getMessage()).c_str()),
                     (unsigned int)-1);
        xmlnode_insert_cdata(xmlnode_insert_tag(x,"url"),
                     it_convert_windows2utf8(xmlnode_pool(msg),
                                 (urlm->getURL()).c_str()),
                     (unsigned int)-1);
        break;
    }
    case MessageEvent::AuthReq: {
	    contact cc;
        AuthReqEvent *autr = static_cast<AuthReqEvent*>(ev);

		log_debug(ZONE,"Authorization request received");

        msg = xmlnode_new_tag("presence");
        xmlnode_insert_cdata(xmlnode_insert_tag(msg,"status"),
                             it_convert_windows2utf8(xmlnode_pool(msg),
                                                     autr->getMessage().c_str()),
                             (unsigned int)-1);

        xmlnode_put_attrib(msg,"type","subscribe");

        /* add to our roster  !!!! */
        cc = it_contact_get(sesja,uin);
        if (cc == NULL)
          it_contact_add(sesja,uin);        
        break;
    }
    case MessageEvent::UserAdd: {
      contact cc;
      msg = xmlnode_new_tag("presence");
      xmlnode_put_attrib(msg,"type","subscribe");
      
      /* add to our roster  !!!! */
      cc = it_contact_get(sesja,uin);
      if (cc == NULL)
        it_contact_add(sesja,uin);
      
      log_debug(ZONE,"User added message recived");
      break;
    }
    case MessageEvent::AuthAck: {
              log_debug(ZONE,"Authorization received");

        AuthAckEvent* ackm=static_cast<AuthAckEvent*>(ev);
        msg = xmlnode_new_tag("presence");
        if(ackm->isGranted())
          xmlnode_put_attrib(msg,"type","subscribed");
        else 
          xmlnode_put_attrib(msg,"type","unsubscribed");
        break;
    }
    case MessageEvent::SMS: {
        log_debug(ZONE,"SMS received");
        SMSMessageEvent* smsm=static_cast<SMSMessageEvent*>(ev);
        
        msg = xmlnode_new_tag("message");
        string text=smsm->getMessage();
        xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
                     text.c_str(),text.size());
        
        xmlnode_put_attrib(msg,"to",jid_full(sesja->id));
        xmlnode_put_attrib(msg,"from",(c->getMobileNo()+
                           "@"+sesja->ti->sms_id).c_str());
        it_deliver(sesja->ti,msg);
        return;
    }
    case MessageEvent::SMS_Receipt: {
        log_debug(ZONE,"SMS receipt received");
        SMSReceiptEvent* receipt=static_cast<SMSReceiptEvent*>(ev);
        
        msg = xmlnode_new_tag("message");
        string text= LNG_SMS_MESSAGE+ 
          ( receipt->delivered() ? 
            string(LNG_SMS_DELIVERED) + receipt->getDeliveryTime() :
            LNG_SMS_NOTDELIVERED );

        xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
							 it_convert_windows2utf8(xmlnode_pool(msg),text.c_str()),
                             (unsigned int)-1);

        xmlnode_put_attrib(msg,"to",jid_full(sesja->id));
        xmlnode_put_attrib(msg,"from",(c->getMobileNo()+
                           "@"+sesja->ti->sms_id).c_str());
        it_deliver(sesja->ti,msg);
        return;
    }
    case MessageEvent::AwayMessage: {
          log_debug(ZONE,"Away message received");
        AwayMessageEvent *aev = static_cast<AwayMessageEvent*>(ev);
        
        sendContactPresence(aev->getContact()->getUIN(),
                            aev->getAwayMessage());
        return;
    }
    case MessageEvent::EmailEx: {
        log_debug(ZONE,"email express received");
        EmailExEvent* eeev=static_cast<EmailExEvent*>(ev);
        
        msg = xmlnode_new_tag("message");
        xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
                     it_convert_windows2utf8(xmlnode_pool(msg),
                                             ((eeev->getSender())+
                                              LNG_EMAIL_1 +
                                              eeev->getEmail() +
                                              LNG_EMAIL_2 +
                                              eeev->getMessage()).c_str()),
                             (unsigned int)-1);
        break;
    }
    case MessageEvent::Email: {
        log_debug(ZONE,"email message received");
        EmailMessageEvent* emev=static_cast<EmailMessageEvent*>(ev);
        
        msg = xmlnode_new_tag("message");
        xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
                     it_convert_windows2utf8(xmlnode_pool(msg),
                                 (emev->getMessage()).c_str()),
                     (unsigned int)-1);
        
        break;
        
    }
	
	case MessageEvent::WebPager: {
        log_debug(ZONE,"email express received");
        WebPagerEvent* ewp=static_cast<WebPagerEvent*>(ev);
        
        msg = xmlnode_new_tag("message");
        xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
							 it_convert_windows2utf8(xmlnode_pool(msg),
                                             (LNG_PAGER_1 +
											  (ewp->getSender())+
                                              LNG_PAGER_2 +
                                              ewp->getEmail() +
                                              LNG_PAGER_3 +
                                              ewp->getMessage()).c_str()),
                             (unsigned int)-1);
	  break;
	}

    default:
      msg = xmlnode_new_tag("message");
      char s[]="You have recived unknown message";
      xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),s,
                   (unsigned int)-1);
      break;
    }
    
    xmlnode_put_attrib(msg,"to",jid_full(sesja->id));
    id = it_uin2jid(xmlnode_pool(msg),uin,sesja->from->server);
    xmlnode_put_attrib(msg,"from",jid_full(id));
    it_deliver(sesja->ti,msg);

    ev->setDelivered(true);
    return;    
}

void WPclient::SignalMessageAck(MessageEvent *ev) {
    if(!ev->isFinished()) return;

	unsigned int uin = 0;
    ContactRef c = ev->getContact();

	if (c->isVirtualContact())
	  uin = 0;
	else
	  uin = c->getUIN();

	if (ev->getType() == ICQ2000::MessageEvent::AwayMessage){
	  AwayMessageEvent *aev = static_cast<AwayMessageEvent*>(ev);
	  log_debug(ZONE,"Away message received");
	  sendContactPresence(uin,aev->getAwayMessage());
	  return;
	}
	
	if(!ev->isDelivered()) {
	  jid id;
	  xmlnode msg;
	  
	  switch(ev->getDeliveryFailureReason()) {
	    
	  case MessageEvent::Failed_Denied: 
	    msg = xmlnode_new_tag("message");	    
	    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
							 it_convert_windows2utf8(xmlnode_pool(msg),LNG_M_ACK_IGNORE),
							 (unsigned int) -1);	    	
	    break;
	  
	  case MessageEvent::Failed_Occupied:
	    msg = xmlnode_new_tag("message");	     
	    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
							 it_convert_windows2utf8(xmlnode_pool(msg),LNG_M_ACK_BUSY),
							 (unsigned int) -1);	   	
	    break;

	  case MessageEvent::Failed_DND:
	    msg = xmlnode_new_tag("message");	     
	    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
							 it_convert_windows2utf8(xmlnode_pool(msg),LNG_M_ACK_DND),
							 (unsigned int) -1);	   	
				break;
	  case MessageEvent::Failed_NotConnected:
	    return;
	    msg = xmlnode_new_tag("message");	     
	    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
							 it_convert_windows2utf8(xmlnode_pool(msg),LNG_M_ACK_NC),
							 (unsigned int) -1); 	
	    break;

	  case MessageEvent::Failed:
	  default:
	    msg = xmlnode_new_tag("message");	     
	    xmlnode_insert_cdata(xmlnode_insert_tag(msg,"body"),
							 it_convert_windows2utf8(xmlnode_pool(msg),LNG_M_ACK_FAIL),
							 (unsigned int) -1);	   	
	  }
	  
	  xmlnode_put_attrib(msg,"to",jid_full(sesja->id));
	  id = it_uin2jid(xmlnode_pool(msg),uin,sesja->from->server);
	  xmlnode_put_attrib(msg,"from",jid_full(id));
	  it_deliver(sesja->ti,msg);
	  return;	
	}

        /* Parsing away messages included in message acks would be nice
         * but does not work with ICQ Lite and some other clients which
         * either put in a "not supported" here or echo the last message
         * sent to them.
	ICQMessageEvent *cev = dynamic_cast<ICQMessageEvent*>(ev);
	if (cev && (cev->getAwayMessage().size())) {
	  log_debug(ZONE,"Away message received with message ack");
	  sendContactPresence(uin,cev->getAwayMessage());
	} */
}

void WPclient::SignalUserInfoChangeEvent(UserInfoChangeEvent *ev) {
  ContactRef c=ev->getContact();

  log_debug(ZONE,"Contact %d changed info",c->getUIN());
}

void WPclient::SignalStatusChangeEvent(StatusChangeEvent *ev) {
  ContactRef c=ev->getContact();

  if (sesja->uin == c->getUIN()) {
    //self event
    return;
  }

  log_debug(ZONE,"Contact %d  changed status",c->getUIN());
  sendContactPresence(c->getUIN());  
}

void WPclient::SignalContactList(ContactListEvent *ev) {
    ContactRef c=ev->getContact();
    switch(ev->getType()) {
    case ContactListEvent::UserAdded:
      log_debug(ZONE,"ICQ UserAdded %d ",c->getUIN());
      break;
    case ContactListEvent::UserRemoved:
      log_debug(ZONE,"ICQ UserRemoved %d",c->getUIN());
      break;
    }
}

void WPclient::SignalServerContactEvent(ServerBasedContactEvent* ev) {
    log_debug(ZONE,"Got server based contact list, importing");
    ContactList l = ev->getContactList();
    ContactList::iterator curr = l.begin();
    while (curr != l.end()) {
      contact c = it_contact_get(sesja,(*curr)->getUIN());
      if (c == NULL) {
        /* new contact not yet in our list */
        c = it_contact_add(sesja,(*curr)->getUIN());
        if(c != NULL)
          it_contact_subscribe(c,(*curr)->getAlias().c_str());
        log_debug(ZONE,"Imported UIN %ul", (*curr)->getUIN());
      } else
        log_debug(ZONE,"Skipped UIN %ul (already in list)", (*curr)->getUIN());
      ++curr;
    }
    log_debug(ZONE,"Finished import");
}

/* want auto resp */
void WPclient::SignalAwayMessageEvent(ICQMessageEvent *ev) {
  if (ev->getType() != MessageEvent::AwayMessage) return;

  log_debug(ZONE,"SignalAwayMessageEvent");

  if (sesja->status_text[0] != '\0') {
    pool p;
    p = pool_heap(2048);
    ev->setAwayMessage(string(
                              it_convert_utf82windows(p,sesja->status_text)
                              ));
    pool_free(p);
  }
}

/* search_result */
void WPclient::SignalSearchResultEvent(SearchResultEvent *ev) {
  meta_gen meta;

  if (search_ev == ev) {
	UIN_t uin;

    if (sesja->pend_search ==  NULL) {
      log_alert(ZONE,"No search at session");
      search_ev = NULL;
      return;
    }

    if (!ev->isExpired()) {
      //      ContactList& cl = ev->getContactList();
      
      ContactRef c = ev->getLastContactAdded();
      if (c.get() != NULL) {
        uin = c->getUIN();
        meta.nick  = (char*)c->getAlias().c_str();
        meta.first = (char*)c->getFirstName().c_str();
        meta.last  = (char*)c->getLastName().c_str();
        meta.email = (char*)c->getEmail().c_str();
        if (c->getAuthReq()) {
          meta.auth = 1;
        } else {
          meta.auth = 0;
        }


		switch (c->getStatus())
		  {
		  case STATUS_OFFLINE:
			meta.status = ICQ_STATUS_OFFLINE;
			break;
		  case STATUS_AWAY:
			meta.status = ICQ_STATUS_AWAY;
			break;
		  case STATUS_DND:
			meta.status = ICQ_STATUS_DND;
			break;
		  case STATUS_NA:
			meta.status = ICQ_STATUS_NA;
			break;
		  case STATUS_OCCUPIED:
			meta.status = ICQ_STATUS_NA;
			break;
		  case STATUS_FREEFORCHAT:
			meta.status = ICQ_STATUS_FREE_CHAT;
			break;
		  default:
			meta.status = ICQ_STATUS_ONLINE;
			break;
		  }
		
        
		log_debug(ZONE,"Search enter part");
        
        (*(meta_search_cb)sesja->pend_search->cb)
          (sesja,uin,&meta,sesja->pend_search->arg);
      }
    }
    else {
      log_alert(ZONE,"search timedout");
      uin = 0;
    }
    
    if (ev->isFinished()) {
      log_debug(ZONE,"Search send");
      (*(meta_search_cb)sesja->pend_search->cb)
        (sesja,uin,NULL,sesja->pend_search->arg);
      search_ev = NULL;
      sesja->pend_search = NULL;
    }
  }
  else {
    log_alert(ZONE,"Not our search event search_ev= %p",search_ev);
  }
}

void WPclient::SignalLogE(LogEvent *ev) {
    switch(ev->getType()) {
    case LogEvent::INFO:
        log_debug(ZONE,"%s\n",ev->getMessage().c_str());
        break;
    case LogEvent::WARN:
        log_warn(ZONE,"%s\n",ev->getMessage().c_str());
        break;
    case LogEvent::PACKET:
    case LogEvent::DIRECTPACKET:
        log_debug(ZONE,"%s\n",ev->getMessage().c_str());
        break;
    case LogEvent::ERROR:
        log_alert(ZONE,"%s\n",ev->getMessage().c_str());
        break;
    }

}

/* to take sockets operations */
void WPclient::Send(Buffer& b) {
  mio_write(sesja->s_mio,NULL,(char *)&(*b.begin()),b.size());
}

void WPclient::SocketDisconnect() {
  mio_close(sesja->s_mio);
}

void WPclient::SocketConnect(const char* host,int port,int type) {
  log_debug(ZONE,"Connect type %d host: %s:%d",type,host,port);

  /* auth */
  if (type == 1) {
    sesja->reference_count++;
    mio_connect((char *)host,port,(void *) it_server_auth,(void *) sesja,60,NULL,NULL);  
  }
  else {
    /* wait for auth socket to close */
    while (sesja->s_mio != NULL) {
      usleep(10);
    }
    
    sesja->reference_count++;
    mio_connect((char *)host,port,(void *) it_server_bos,(void *) sesja,60,NULL,NULL);  
  } 
}







