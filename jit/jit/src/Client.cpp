/*
 * libICQ2000 Client
 *
 * Copyright (C) 2001 Barnaby Gray <barnaby@beedesign.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */

/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

#include <libicq2000/TLV.h>
#include <libicq2000/UserInfoBlock.h>

#include <libicq2000/Client.h>
#include <libicq2000/SNAC.h>

#include "sstream_fix.h"

using std::ostringstream;
using std::endl;

namespace ICQ2000 {

  /* signal messages */
  void DCCache_expired_cb(Client * client,DirectClient *dc) {
    if (client ==  NULL) return;
  }


  void ICBMCookieCache_expired_cb(Client * client,MessageEvent *ev) {
    if (client ==  NULL) return;

    client->ICBMCookieCache_expired_cb(ev);
  }

  void reqidcache_expired_cb(Client * client,RequestIDCacheValue* v) {
    if (client ==  NULL) return;

    client->reqidcache_expired_cb(v);
  }

  void contactlist_signal_cb(Client *client, int type,ContactListEvent* ev) {
    if (client ==  NULL) return;

    if (type == 1)
      client->contactlist_cb(ev);
    else
      if (type == 2)
	client->visiblelist_cb(ev);
    else
      if (type == 3)
	client->invisiblelist_cb(ev);
  }

  void status_change_signal_cb(Client * client, StatusChangeEvent *ev) {
    if (client !=  NULL)
      client->SignalStatusChangeEvent(ev);
  }

  void userinfo_change_signal_cb(Client * client, UserInfoChangeEvent *ev) {
    if (client !=  NULL)
      client->SignalUserInfoChangeEvent(ev);
  }

  void messaged_cb(Client * client ,MessageEvent* ev) {
    if (client ==  NULL) return;

    client->SignalMessaged(ev);
  }

  void messageack_cb(Client * client ,MessageEvent* ev) {
    if (client ==  NULL) return;

    client->SignalMessageAck(ev);
  }

  void want_auto_resp_cb(Client * client ,ICQMessageEvent* ev) {
    if (client ==  NULL) return;

    client->SignalAwayMessageEvent(ev);
  }

  void logger_cb(Client * client ,LogEvent* ev) {
    if (client ==  NULL) return;

    client->SignalLogE(ev);
  }
   

  /**
   *  Constructor for creating the Client object.  Use this when
   *  uin/password are unavailable at time of creation, they can
   *  always be set later.
   */
  Client::Client()
    : m_self( new Contact(1) ),
      m_message_handler(m_self, &m_contact_list),
      m_recv(&m_translator)
  {
    Init();
  }

  /**
   *  Constructor for creating the Client object.  Use this when the
   *  uin/password are available at time of creation, to save having
   *  to set them later.
   *
   *  @param uin the owner's uin
   *  @param password the owner's password
   */
  Client::Client(const unsigned int uin, const string& password)
    : m_self( new Contact(uin) ), m_password(password),
      m_message_handler(m_self, &m_contact_list),
      m_recv(&m_translator)
  {
    Init();
  }

  /**
   *  Destructor for the Client object.  This will free up all
   *  resources used by Client, including any Contact objects. It also
   *  automatically disconnects if you haven't done so already.
   */
  Client::~Client() {
    if (m_cookie_data)
      delete [] m_cookie_data;
    Disconnect(DisconnectedEvent::REQUESTED);
  }

  void Client::Init() {
    m_authorizerHostname = "login.icq.com";
    m_authorizerPort = 5190;
    m_bosOverridePort = false;
    m_state = NOT_CONNECTED;
    
    m_cookie_data = NULL;
    m_cookie_length = 0;

    m_self->setStatus(STATUS_OFFLINE, false);

    m_status_wanted = STATUS_OFFLINE;
    m_invisible_wanted = false;
    m_web_aware = false;

    m_ext_ip = 0;
    m_use_portrange = false;
    m_lower_port = 0;
    m_upper_port = 0;

    m_cookiecache.setDefaultTimeout(30);
    // 30 seconds is hopefully enough for even the slowest connections
    m_cookiecache.setClient(this);
    //    m_cookiecache.expired.connect( slot(this,&Client::ICBMCookieCache_expired_cb) );


    m_reqidcache.setClient(this);
    //    m_reqidcache.expired.connect( slot(this, &Client::reqidcache_expired_cb) );
    

    /* contact list callbacks */
    m_contact_list.setClient(this,1);
    //    m_contact_list.contactlist_signal.connect( slot(this, &Client::contactlist_cb) );
    
    /* visible, invisible lists callbacks */
    m_visible_list.setClient(this,2);
    m_invisible_list.setClient(this,3);
    //    m_visible_list.contactlist_signal.connect( slot(this, &Client::visiblelist_cb) );
    //    m_invisible_list.contactlist_signal.connect( slot(this, &Client::invisiblelist_cb) );

    /* self contact callbacks */
    m_self->setClient(this);
      //    m_self->status_change_signal.connect( self_contact_status_change_signal.slot() );
    //    m_self->userinfo_change_signal.connect( self_contact_userinfo_change_signal.slot() );
    
    /* message handler callbacks */
    m_message_handler.setClient(this);
    //    m_message_handler.messaged.connect( messaged.slot() );
    //    m_message_handler.messageack.connect( messageack.slot() );
    //    m_message_handler.want_auto_resp.connect( want_auto_resp.slot() );
    //    m_message_handler.logger.connect( logger.slot() );
  }

  unsigned short Client::NextSeqNum() {
    m_client_seq_num = ++m_client_seq_num & 0x7fff;
    return m_client_seq_num;
  }

  unsigned int Client::NextRequestID() {
    m_requestid = ++m_requestid & 0x7fffffff;
    return m_requestid;
  }

  void Client::ConnectAuthorizer(State state) {
    SignalLog(LogEvent::INFO, "Client connecting");

    SocketConnect(m_authorizerHostname.c_str(),m_authorizerPort,1);

    // randomize sequence number
    srand(time(0));
    m_client_seq_num = (unsigned short)(0x7fff*(rand()/(RAND_MAX+1.0)));
    m_requestid = (unsigned int)(0x7fffffff*(rand()/(RAND_MAX+1.0)));

    m_state = state;
  }

  void Client::DisconnectAuthorizer() {
    SocketDisconnect();
    m_state = NOT_CONNECTED;
  }

  void Client::ConnectBOS() {

    SocketConnect(m_bosHostname.c_str(),m_bosPort,2);

    m_state = BOS_AWAITING_CONN_ACK;
  }

  void Client::DisconnectBOS() {
    m_state = NOT_CONNECTED;

    SocketDisconnect();
  }

  // ------------------ Signal Dispatchers -----------------
  void Client::SignalConnect() {
    m_state = BOS_LOGGED_IN;
    ConnectedEvent ev;
    SignalConnected(&ev);
      //    connected.emit(&ev);
  }

  void Client::SignalDisconnect(DisconnectedEvent::Reason r) {
    DisconnectedEvent ev(r);
    SignalDisconnected(&ev);
    //    disconnected.emit(&ev);

    if (m_self->getStatus() != STATUS_OFFLINE) {
      m_self->setStatus(STATUS_OFFLINE, false);
    }

    // ensure all contacts return to Offline
    ContactList::iterator curr = m_contact_list.begin();
    while(curr != m_contact_list.end()) {
      Status old_st = (*curr)->getStatus();
      if ( old_st != STATUS_OFFLINE ) {
	(*curr)->setStatus(STATUS_OFFLINE, false);
      }
      ++curr;
    }
  }

  void Client::SignalMessage(MessageSNAC *snac) {
    ContactRef contact;
    ICQSubType *st = snac->getICQSubType();
    if (st == NULL) return;

    bool ack = m_message_handler.handleIncoming( st );
    if (ack) SendAdvancedACK(snac);
  }


  void Client::SignalMessageACK(MessageACKSNAC *snac) {
    UINICQSubType *st = snac->getICQSubType();
    if (st == NULL) return;

    unsigned char type = st->getType();
    switch(type) {
    case MSG_Type_Normal:
    case MSG_Type_URL:
    case MSG_Type_AutoReq_Away:
    case MSG_Type_AutoReq_Occ:
    case MSG_Type_AutoReq_NA:
    case MSG_Type_AutoReq_DND:
    case MSG_Type_AutoReq_FFC:
    {
      ICBMCookie c = snac->getICBMCookie();
      if ( m_cookiecache.exists( c ) ) {
	MessageEvent *ev = m_cookiecache[c];
	ev->setDirect(false);
	m_message_handler.handleIncomingACK( ev, st );
	m_cookiecache.remove(c);
      } else {
	SignalLog(LogEvent::WARN, "Received ACK for unknown message");
      }
    }
    
    break;

    default:
      SignalLog(LogEvent::WARN, "Received ACK for unknown message type");
    }
    

  }

  void Client::SignalMessageOfflineUser(MessageOfflineUserSNAC *snac) {
    /**
     *  Mmm.. it'd be nice to use this as an ack for messages but it's
     *  not consistently sent for all messages through the server
     *  doesn't seem to be sent for normal (non-advanced) messages to
     *  online users.
     */
    ICBMCookie c = snac->getICBMCookie();

    if ( m_cookiecache.exists( c ) ) {

      /* indicate sending through server */
      MessageEvent *ev = m_cookiecache[c];
      ev->setFinished(false);
      ev->setDelivered(false);
      ev->setDirect(false);
      SignalMessageAck(ev);
      //      messageack.emit(ev);

    } else {
      SignalLog(LogEvent::WARN, "Received Offline ACK for unknown message");
    }
  }

  void Client::SignalSrvResponse(SrvResponseSNAC *snac) {
    if (snac->getType() == SrvResponseSNAC::OfflineMessagesComplete) {

      /* We are now meant to ACK this to say
       * the we have got the offline messages
       * and the server can dispose of storing
       * them
       */
      SendOfflineMessagesACK();

    } else if (snac->getType() == SrvResponseSNAC::OfflineMessage) {

      // wow.. this is so much simpler now :-)
      m_message_handler.handleIncoming(snac->getICQSubType(), snac->getTime());
      
    } else if (snac->getType() == SrvResponseSNAC::SMS_Error) {
      // mmm
    } else if (snac->getType() == SrvResponseSNAC::SMS_Response) {
      
      unsigned int reqid = snac->RequestID();
      if ( m_reqidcache.exists( reqid ) ) {
		RequestIDCacheValue *v = m_reqidcache[ reqid ];
		
		if ( v->getType() == RequestIDCacheValue::SMSMessage ) {
		  SMSEventCacheValue *uv = static_cast<SMSEventCacheValue*>(v);
		  SMSMessageEvent *ev = uv->getEvent();
		  
		  if (snac->deliverable()) {
			ev->setFinished(true);
			ev->setDelivered(true);
			ev->setDirect(false);
			SignalMessageAck(ev);
			//	    messageack.emit(ev);
			m_reqidcache.remove( reqid );
		  } else if (snac->smtp_deliverable()) {
			
			// todo - konst have volunteered :-)
			//                  yeah I did.. <konst> ;)
			
			//	    ev->setSMTPFrom(snac->getSMTPFrom());
			//	    ev->setSMTPTo(snac->getSMTPTo());
			//	    ev->setSMTPSubject(snac->getSMTPSubject());
			
			//	    m_smtp.SendEvent(ev);
			
		  } else {
			if (snac->getErrorParam() != "DUPLEX RESPONSE") {
	      // ignore DUPLEX RESPONSE since I always get that
			  ev->setFinished(true);
			  ev->setDelivered(false);
			  ev->setDirect(false);
			  ev->setDeliveryFailureReason(MessageEvent::Failed);
			  SignalMessageAck(ev);
			  //	      messageack.emit(ev);
			  m_reqidcache.remove( reqid );
			}
		  }
		  
		} else {
		  throw ParseException("Request ID cached value is not for an SMS Message");
		}
      } else {
		throw ParseException("Received an SMS response for unknown request id");
      }
      
    } else if (snac->getType() == SrvResponseSNAC::SimpleUserInfo) {
	  
      if ( m_reqidcache.exists( snac->RequestID() ) ) {
		RequestIDCacheValue *v = m_reqidcache[ snac->RequestID() ];

		if ( v->getType() == RequestIDCacheValue::Search ) {
		  SearchCacheValue *sv = static_cast<SearchCacheValue*>(v);
		  
		  SearchResultEvent *ev = sv->getEvent();
		  if (snac->isEmptyContact()) {
			ev->setLastContactAdded( NULL );
		  } else {
			ContactRef c = new Contact( snac->getUIN() );
			c->setAlias(snac->getAlias());
			c->setFirstName(snac->getFirstName());
			c->setLastName(snac->getLastName());
			c->setEmail(snac->getEmail());
			c->setStatus(snac->getStatus(), false);
			c->setAuthReq(snac->getAuthReq());
			ContactList& cl = ev->getContactList();
			ev->setLastContactAdded( cl.add(c) );
			
			if (snac->isLastInSearch())
			  ev->setNumberMoreResults( snac->getNumberMoreResults() );
			
		  }
		  
		  if (snac->isLastInSearch()) ev->setFinished(true);
		  
		  SignalSearchResultEvent(ev);
		  //	  search_result.emit(ev);
		  
		  if (ev->isFinished()) {
			delete ev;
			m_reqidcache.remove( snac->RequestID() );
		  }
		  
		} else {
		  SignalLog(LogEvent::WARN, "Request ID cached value is not for a Search request");
		}
		
      } else {
		if ( m_contact_list.exists( snac->getUIN() ) ) {
		  // update Contact
		  ContactRef c = m_contact_list[ snac->getUIN() ];
		  c->setAlias( snac->getAlias() );
		  c->setEmail( snac->getEmail() );
		  c->setFirstName( snac->getFirstName() );
		  c->setLastName( snac->getLastName() );
		}
      }
      
    } else if (snac->getType() == SrvResponseSNAC::SearchSimpleUserInfo) {

      if ( m_reqidcache.exists( snac->RequestID() ) ) {
		RequestIDCacheValue *v = m_reqidcache[ snac->RequestID() ];
		
		if ( v->getType() == RequestIDCacheValue::Search ) {
		  SearchCacheValue *sv = static_cast<SearchCacheValue*>(v);
		  
		  SearchResultEvent *ev = sv->getEvent();
		  if (snac->isEmptyContact()) {
			ev->setLastContactAdded( NULL );
		  } else {
			ContactRef c = new Contact( snac->getUIN() );
			c->setAlias(snac->getAlias());
			c->setFirstName(snac->getFirstName());
			c->setLastName(snac->getLastName());
			c->setEmail(snac->getEmail());
			c->setStatus(snac->getStatus(), false);
			c->setAuthReq(snac->getAuthReq());
			ContactList& cl = ev->getContactList();
			ev->setLastContactAdded( cl.add(c) );
			
			if (snac->isLastInSearch())
			  ev->setNumberMoreResults( snac->getNumberMoreResults() );
			
		  }
		  
		  if (snac->isLastInSearch()) ev->setFinished(true);
		  
		  SignalSearchResultEvent(ev);
		  //	  search_result.emit(ev);
		  
		  if (ev->isFinished()) {
			delete ev;
			m_reqidcache.remove( snac->RequestID() );
		  }
		  
		} else {
		  SignalLog(LogEvent::WARN, "Request ID cached value is not for a Search request");
		}
		
      } else {
		SignalLog(LogEvent::WARN, "Received a Search Result for unknown request id");
      }
	  
    } else if (snac->getType() == SrvResponseSNAC::RMainHomeInfo) {
	  
      try {
		ContactRef c = getUserInfoCacheContact( snac->RequestID() );
		c->setMainHomeInfo( snac->getMainHomeInfo() );
		c->userinfo_change_emit();
      } catch(ParseException e) {
		SignalLog(LogEvent::WARN, e.what());
      }
	  
    } else if (snac->getType() == SrvResponseSNAC::RHomepageInfo) {
	  
      try {
		ContactRef c = getUserInfoCacheContact( snac->RequestID() );
		c->setHomepageInfo( snac->getHomepageInfo() );
		c->userinfo_change_emit();
      } catch(ParseException e) {
		SignalLog(LogEvent::WARN, e.what());
      }
	  
    } else if (snac->getType() == SrvResponseSNAC::RWorkInfo) {
	  
      try {
		ContactRef c = getUserInfoCacheContact( snac->RequestID() );
		c->setWorkInfo( snac->getWorkInfo() );
		c->userinfo_change_emit();
      } catch(ParseException e) {
		SignalLog(LogEvent::WARN, e.what());
      }

    } else if (snac->getType() == SrvResponseSNAC::RBackgroundInfo) {
	  
      try {
		ContactRef c = getUserInfoCacheContact( snac->RequestID() );
		c->setBackgroundInfo( snac->getBackgroundInfo() );
		c->userinfo_change_emit();
      } catch(ParseException e) {
		SignalLog(LogEvent::WARN, e.what());
      }
	  
    } else if (snac->getType() == SrvResponseSNAC::RInterestInfo) {
	  
      try {
		ContactRef c = getUserInfoCacheContact( snac->RequestID() );
		c->setInterestInfo( snac->getPersonalInterestInfo() );
		c->userinfo_change_emit();
      } catch(ParseException e) {
		SignalLog(LogEvent::WARN, e.what());
      }
	  
    } else if (snac->getType() == SrvResponseSNAC::REmailInfo) {
	  
      try {
		ContactRef c = getUserInfoCacheContact( snac->RequestID() );
		c->setEmailInfo( snac->getEmailInfo() );
		c->userinfo_change_emit();
      } catch(ParseException e) {
		SignalLog(LogEvent::WARN, e.what());
      }
	  
    } else if (snac->getType() == SrvResponseSNAC::RAboutInfo) {
	  
      try {
		ContactRef c = getUserInfoCacheContact( snac->RequestID() );
		c->setAboutInfo( snac->getAboutInfo() );
      } catch(ParseException e) {
		SignalLog(LogEvent::WARN, e.what());
      }

    }
  }
  
  ContactRef Client::getUserInfoCacheContact(unsigned int reqid) {

    if ( m_reqidcache.exists( reqid ) ) {
      RequestIDCacheValue *v = m_reqidcache[ reqid ];

      if ( v->getType() == RequestIDCacheValue::UserInfo ) {
		UserInfoCacheValue *uv = static_cast<UserInfoCacheValue*>(v);
		return uv->getContact();
      }
      else throw ParseException("Request ID cached value is not for a User Info request");

    } else {
      throw ParseException("Received a UserInfo response for unknown request id");
    }

  }

  void Client::SignalUINResponse(UINResponseSNAC *snac) {
    unsigned int uin = snac->getUIN();
    NewUINEvent e(uin);
    //    newuin.emit(&e);
    SignalNewUIN(&e);
  }

  void Client::SignalUINRequestError() {
    NewUINEvent e(0,false);
    SignalNewUIN(&e);
    //    newuin.emit(&e);
  }
  
  void Client::SignalRateInfoChange(RateInfoChangeSNAC *snac) {
    RateInfoChangeEvent e(snac->getCode(), snac->getRateClass(),
			  snac->getWindowSize(), snac->getClear(),
			  snac->getAlert(), snac->getLimit(),
			  snac->getDisconnect(), snac->getCurrentAvg(),
			  snac->getMaxAvg());
    SignalRateEvent(&e);
    //    rate.emit(&e);
  }

  void Client::SignalLog(LogEvent::LogType type, const string& msg) {
    LogEvent ev(type,msg);
    SignalLogE(&ev);
    //    logger.emit(&ev);
  }
  
  void Client::ICBMCookieCache_expired_cb(MessageEvent *ev) {
    SignalLog(LogEvent::WARN, "Message timeout without receiving ACK, sending offline");
    SendViaServerNormal(ev);
    /* downgrade Contact's capabilities, so we don't
       attempt to send it as advanced again           */
    ev->getContact()->set_capabilities(Capabilities());
  }

  void Client::reqidcache_expired_cb(RequestIDCacheValue* v) 
  {
    if ( v->getType() == RequestIDCacheValue::Search ) {
      SearchCacheValue *sv = static_cast<SearchCacheValue*>(v);

      SearchResultEvent *ev = sv->getEvent();
      ev->setLastContactAdded( NULL );
      ev->setExpired(true);
      ev->setFinished(true);
      SignalSearchResultEvent(ev);
	//      search_result.emit(ev);
      delete ev;	  
    }
  }
  

  void Client::SignalUserOnline(BuddyOnlineSNAC *snac) {
    const UserInfoBlock& userinfo = snac->getUserInfo();
    if (m_contact_list.exists(userinfo.getUIN())) {
      ContactRef c = m_contact_list[userinfo.getUIN()];
      Status old_st = c->getStatus();
      
      c->setDirect(true); // reset flags when a user goes online
      c->setStatus( Contact::MapICQStatusToStatus(userinfo.getStatus()),
		    Contact::MapICQStatusToInvisible(userinfo.getStatus()) );
      c->setExtIP( userinfo.getExtIP() );
      c->setLanIP( userinfo.getLanIP() );
      c->setExtPort( userinfo.getExtPort() );
      c->setLanPort( userinfo.getLanPort() );
      c->setTCPVersion( userinfo.getTCPVersion() );
      c->set_signon_time( userinfo.getSignonDate() );
      if (userinfo.contains_capabilities())
		c->set_capabilities( userinfo.get_capabilities() );
      
      ostringstream ostr;
      ostr << "Received Buddy Online for "
	   << c->getAlias()
	   << " (" << c->getUIN() << ") " << Status_text[old_st]
	   << "->" << c->getStatusStr() << " from server";
      SignalLog(LogEvent::INFO, ostr.str() );

    } else {
      ostringstream ostr;
      ostr << "Received Status change for user not on contact list: " << userinfo.getUIN();
      SignalLog(LogEvent::WARN, ostr.str());
    }
  }

  void Client::SignalUserOffline(BuddyOfflineSNAC *snac) {
    const UserInfoBlock& userinfo = snac->getUserInfo();
    if (m_contact_list.exists(userinfo.getUIN())) {
      ContactRef c = m_contact_list[userinfo.getUIN()];
      c->setStatus(STATUS_OFFLINE, false);

      ostringstream ostr;
      ostr << "Received Buddy Offline for "
	   << c->getAlias()
	   << " (" << c->getUIN() << ") from server";
      SignalLog(LogEvent::INFO, ostr.str() );
    } else {
      ostringstream ostr;
      ostr << "Received Status change for user not on contact list: " << userinfo.getUIN();
      SignalLog(LogEvent::WARN, ostr.str());
    }
  }

  // ------------------ Outgoing packets -------------------

  Buffer::marker Client::FLAPHeader(Buffer& b, unsigned char channel) 
  {
    b.setBigEndian();
    b << (unsigned char) 42;
    b << channel;
    b << NextSeqNum();
    Buffer::marker mk = b.getAutoSizeShortMarker();
    return mk;
  }
  
  void Client::FLAPFooter(Buffer& b, Buffer::marker& mk) 
  {
    b.setAutoSizeMarker(mk);
  }
  

  void Client::FLAPwrapSNAC(Buffer& b, const OutSNAC& snac)
  {
    Buffer::marker mk = FLAPHeader(b, 0x02);
    b << snac;
    FLAPFooter(b,mk);
  }

  void Client::FLAPwrapSNACandSend(const OutSNAC& snac)
  {
    Buffer b(&m_translator);
    FLAPwrapSNAC(b, snac);
    Send(b);
  }
  
  void Client::SendAuthReq() {
    Buffer b(&m_translator);
    Buffer::marker mk = FLAPHeader(b,0x01);

    b << (unsigned int)0x00000001;

    b << ScreenNameTLV(m_self->getStringUIN())
      << PasswordTLV(m_password)
      << ClientProfileTLV("ICQ Inc. - Product of ICQ (TM).2000b.4.63.1.3279.85")
      << ClientTypeTLV(266)
      << ClientVersionMajorTLV(4)
      << ClientVersionMinorTLV(63)
      << ClientICQNumberTLV(1)
      << ClientBuildMajorTLV(3279)
      << ClientBuildMinorTLV(85)
      << LanguageTLV("en")
      << CountryCodeTLV("us");

    FLAPFooter(b,mk);
    SignalLog(LogEvent::INFO, "Sending Authorisation Request");
    Send(b);
  }

  void Client::SendNewUINReq() {
    Buffer b(&m_translator);
    Buffer::marker mk;

    mk = FLAPHeader(b,0x01);
    b << (unsigned int)0x00000001;
    FLAPFooter(b,mk);
    Send(b);

    SignalLog(LogEvent::INFO, "Sending New UIN Request");
    FLAPwrapSNACandSend( UINRequestSNAC(m_password) );
  }
    
  void Client::SendCookie() {
    Buffer b(&m_translator);
    Buffer::marker mk = FLAPHeader(b,0x01);

    b << (unsigned int)0x00000001;

    b << CookieTLV(m_cookie_data, m_cookie_length);

    FLAPFooter(b,mk);
    SignalLog(LogEvent::INFO, "Sending Login Cookie");
    Send(b);
  }
    
  void Client::SendCapabilities() {
    SignalLog(LogEvent::INFO, "Sending Capabilities");
    FLAPwrapSNACandSend( CapabilitiesSNAC() );
  }

  void Client::SendSetUserInfo() {
    SignalLog(LogEvent::INFO, "Sending Set User Info");
    FLAPwrapSNACandSend( SetUserInfoSNAC() );
  }

  void Client::SendRateInfoRequest() {
    SignalLog(LogEvent::INFO, "Sending Rate Info Request");
    FLAPwrapSNACandSend( RequestRateInfoSNAC() );
  }
  
  void Client::SendRateInfoAck() {
    SignalLog(LogEvent::INFO, "Sending Rate Info Ack");
    FLAPwrapSNACandSend( RateInfoAckSNAC() );
  }

  void Client::SendPersonalInfoRequest() {
    SignalLog(LogEvent::INFO, "Sending Personal Info Request");
    FLAPwrapSNACandSend( PersonalInfoRequestSNAC() );
  }

  void Client::SendAddICBMParameter() {
    SignalLog(LogEvent::INFO, "Sending Add ICBM Parameter");
    FLAPwrapSNACandSend( MsgAddICBMParameterSNAC() );
  }

  void Client::SendLogin() {
    Buffer b(&m_translator);

    if (!m_contact_list.empty())
      FLAPwrapSNAC(b, AddBuddySNAC(m_contact_list) );

    if (m_invisible_wanted)
      FLAPwrapSNAC(b, AddVisibleSNAC(m_visible_list) );
        
    SetStatusSNAC sss(Contact::MapStatusToICQStatus(m_status_wanted, m_invisible_wanted), m_web_aware);

    sss.setSendExtra(true);
    sss.setIP(0);
    sss.setPort( 0 );
    FLAPwrapSNAC( b, sss );

    if (!m_invisible_wanted)
      FLAPwrapSNAC(b, AddInvisibleSNAC(m_invisible_list) );

    FLAPwrapSNAC( b, ClientReadySNAC() );

    FLAPwrapSNAC( b, SrvRequestOfflineSNAC(m_self->getUIN()) );

    SignalLog(LogEvent::INFO, "Sending Contact List, Status, Client Ready and Offline Messages Request");
    Send(b);

    SignalConnect();
    m_last_server_ping = time(NULL);
  }

  void Client::SendOfflineMessagesRequest() {
    SignalLog(LogEvent::INFO, "Sending Offline Messages Request");
    FLAPwrapSNACandSend( SrvRequestOfflineSNAC(m_self->getUIN()) );
  }


  void Client::SendOfflineMessagesACK() {
    SignalLog(LogEvent::INFO, "Sending Offline Messages ACK");
    FLAPwrapSNACandSend( SrvAckOfflineSNAC(m_self->getUIN()) );
  }

  void Client::SendAdvancedACK(MessageSNAC *snac) {
    ICQSubType *st = snac->getICQSubType();
    if (st == NULL || dynamic_cast<UINICQSubType*>(st) == NULL ) return;
    UINICQSubType *ust = dynamic_cast<UINICQSubType*>(snac->grabICQSubType());

    SignalLog(LogEvent::INFO, "Sending Advanced Message ACK");
    FLAPwrapSNACandSend( MessageACKSNAC( snac->getICBMCookie(), ust ) );
  }

  // ------------------ Incoming packets -------------------

  void Client::RecvFromServer(const unsigned char *buf, int len) {
    m_recv.Pack(buf,len);

    try {
      Parse();      
    } catch(ParseException e) {
      SignalLog(LogEvent::ERROR, e.what());
    }
  }

  void Client::Parse() {
    
    // -- FLAP header --

    unsigned char start_byte, channel;
    unsigned short seq_num, data_len;

    // process FLAP(s) in packet

    if (m_recv.empty()) return;

    while (!m_recv.empty()) {
      m_recv.setPos(0);

      m_recv >> start_byte;
      if (start_byte != 42) {
	m_recv.clear();
	SignalLog(LogEvent::WARN, "Invalid Start Byte on FLAP");
	return;
      }

      /* if we don't have at least six bytes we don't have enough
       * info to determine if we have the whole of the FLAP
       */
      if (m_recv.remains() < 5) return;
      
      m_recv >> channel;
      m_recv >> seq_num; // check sequence number - todo
      
      m_recv >> data_len;
      if (m_recv.remains() < data_len) return; // waiting for more of the FLAP

      /* Copy into another Buffer which is passed
       * onto the separate parse code that way
       * multiple FLAPs in one packet are split up
       */
      Buffer sb(&m_translator);
      m_recv.chopOffBuffer( sb, data_len+6 );

      //      {
      //	ostringstream ostr;
      //	ostr << "Received packet from Server" << endl << sb;
      //	SignalLog(LogEvent::PACKET, ostr.str());
      //      }

      sb.advance(6);

      // -- FLAP body --
      
      ostringstream ostr;
      
      switch(channel) {
      case 1:
	ParseCh1(sb,seq_num);
	break;
      case 2:
	ParseCh2(sb,seq_num);
	break;
      case 3:
	ParseCh3(sb,seq_num);
	break;
      case 4:
	ParseCh4(sb,seq_num);
	break;
      default:
	ostr << "FLAP on unrecognised channel 0x" << std::hex << (int)channel;
	SignalLog(LogEvent::WARN, ostr.str());
	break;
      }

      if (sb.beforeEnd()) {
	/* we assert that parsing code eats uses all data
	 * in the FLAP - seems useful to know when they aren't
	 * as it probably means they are faulty
	 */
	ostringstream ostr;
	ostr  << "Buffer pointer not at end after parsing FLAP was: 0x" << std::hex << sb.pos()
	      << " should be: 0x" << sb.size() << " on channel 0x" << std::hex << (int)channel;
	SignalLog(LogEvent::WARN, ostr.str());
      }
      
    }

  }

  void Client::ParseCh1(Buffer& b, unsigned short seq_num) {

    if (b.remains() == 4 && (m_state == AUTH_AWAITING_CONN_ACK || 
			     m_state == UIN_AWAITING_CONN_ACK)) {

      // Connection Acknowledge - first packet from server on connection
      unsigned int unknown;
      b >> unknown; // always 0x0001

      if (m_state == AUTH_AWAITING_CONN_ACK) {
	SendAuthReq();
	SignalLog(LogEvent::INFO, "Connection Acknowledge from server");
	m_state = AUTH_AWAITING_AUTH_REPLY;
      } else if (m_state == UIN_AWAITING_CONN_ACK) {
	SendNewUINReq();
	SignalLog(LogEvent::INFO, "Connection Acknowledge from server");
	m_state = UIN_AWAITING_UIN_REPLY;
      }

    } else if (b.remains() == 4 && m_state == BOS_AWAITING_CONN_ACK) {

      SignalLog(LogEvent::INFO, "Connection Acknowledge from server");

      // Connection Ack, send the cookie
      unsigned int unknown;
      b >> unknown; // always 0x0001

      SendCookie();
      m_state = BOS_AWAITING_LOGIN_REPLY;

    } else {
      SignalLog(LogEvent::WARN, "Unknown packet received on channel 0x01");
    }

  }

  void Client::ParseCh2(Buffer& b, unsigned short seq_num) {
    InSNAC *snac;
    try {
      snac = ParseSNAC(b);
    } catch(ParseException e) {
      ostringstream ostr;
      ostr << "Problem parsing SNAC: " << e.what();
      SignalLog(LogEvent::WARN, ostr.str());
      return;
    }

    switch(snac->Family()) {
      
    case SNAC_FAM_GEN:
      switch(snac->Subtype()) {
      case SNAC_GEN_ServerReady:
	SignalLog(LogEvent::INFO, "Received Server Ready from server");
	SendCapabilities();
	break;
      case SNAC_GEN_RateInfo:
	SignalLog(LogEvent::INFO, "Received Rate Information from server");
	SendRateInfoAck();
	SendPersonalInfoRequest();
	SendAddICBMParameter();
	SendSetUserInfo();
	SendLogin();
	break;
      case SNAC_GEN_CapAck:
	SignalLog(LogEvent::INFO, "Received Capabilities Ack from server");
	SendRateInfoRequest();
	break;
      case SNAC_GEN_UserInfo:
	SignalLog(LogEvent::INFO, "Received User Info from server");
	HandleUserInfoSNAC(static_cast<UserInfoSNAC*>(snac));
	break;
      case SNAC_GEN_MOTD:
	SignalLog(LogEvent::INFO, "Received MOTD from server");
	break;
      case SNAC_GEN_RateInfoChange:
	SignalLog(LogEvent::INFO, "Received Rate Info Change from server");
	SignalRateInfoChange(static_cast<RateInfoChangeSNAC*>(snac));
	break;
      }
      break;

    case SNAC_FAM_BUD:
      switch(snac->Subtype()) {
      case SNAC_BUD_Online:
	SignalUserOnline(static_cast<BuddyOnlineSNAC*>(snac));
	break;
      case SNAC_BUD_Offline:
	SignalUserOffline(static_cast<BuddyOfflineSNAC*>(snac));
	break;
      }
      break;

    case SNAC_FAM_MSG:
      switch(snac->Subtype()) {
      case SNAC_MSG_Message:
	SignalLog(LogEvent::INFO, "Received Message from server");
	SignalMessage(static_cast<MessageSNAC*>(snac));
	break;
      case SNAC_MSG_MessageACK:
	SignalLog(LogEvent::INFO, "Received Message ACK from server");
	SignalMessageACK(static_cast<MessageACKSNAC*>(snac));
	break;
      case SNAC_MSG_OfflineUser:
	SignalLog(LogEvent::INFO, "Received Message to Offline User from server");
	SignalMessageOfflineUser(static_cast<MessageOfflineUserSNAC*>(snac));
	break;
      }
      break;

    case SNAC_FAM_SRV:
      switch(snac->Subtype()) {
      case SNAC_SRV_Response:
	SignalLog(LogEvent::INFO, "Received Server Response from server");
	SignalSrvResponse(static_cast<SrvResponseSNAC*>(snac));
	break;
      }
      break;
    case SNAC_FAM_UIN:
      switch(snac->Subtype()) {
      case SNAC_UIN_Response:
	SignalLog(LogEvent::INFO, "Received UIN Response from server");
	SignalUINResponse(static_cast<UINResponseSNAC*>(snac));
	break;
      case SNAC_UIN_RequestError:
	SignalLog(LogEvent::ERROR, "Received UIN Request Error from server");
	SignalUINRequestError();
	break;
      }
      break;
    case SNAC_FAM_SBL:
      switch(snac->Subtype()) {
      case SNAC_SBL_List_From_Server:
	SignalLog(LogEvent::INFO, "Received server-based list from server\n");
        SBLListSNAC *sbs = static_cast<SBLListSNAC*>(snac);
        SignalServerBasedContactList( sbs->getContactList() );
	break;
      }
      break;
	
	
    } // switch(Family)

    if (dynamic_cast<RawSNAC*>(snac)) {
      ostringstream ostr;
      ostr << "Unknown SNAC packet received - Family: 0x" << std::hex << snac->Family()
	   << " Subtype: 0x" << snac->Subtype();
      SignalLog(LogEvent::WARN, ostr.str());
    }

    delete snac;

  }

  void Client::ParseCh3(Buffer& b, unsigned short seq_num) {
    SignalLog(LogEvent::INFO, "Received packet on channel 0x03");
  }

  void Client::ParseCh4(Buffer& b, unsigned short seq_num) {
    if (m_state == AUTH_AWAITING_AUTH_REPLY || m_state == UIN_AWAITING_UIN_REPLY) {
      // An Authorisation Reply / Error
      TLVList tlvlist;
      tlvlist.Parse(b, TLV_ParseMode_Channel04, (short unsigned int)-1);

      if (tlvlist.exists(TLV_Cookie) && tlvlist.exists(TLV_Redirect)) {

	RedirectTLV *r = static_cast<RedirectTLV*>(tlvlist[TLV_Redirect]);
	ostringstream ostr;
	ostr << "Redirected to: " << r->getHost();
	if (r->getPort() != 0) ostr << " port: " << std::dec << r->getPort();
	SignalLog(LogEvent::INFO, ostr.str());

	m_bosHostname = r->getHost();
	if (!m_bosOverridePort)	{
	  if (r->getPort() != 0) m_bosPort = r->getPort();
	  else m_bosPort = m_authorizerPort;
	}

	// Got our cookie - yum yum :-)
	CookieTLV *t = static_cast<CookieTLV*>(tlvlist[TLV_Cookie]);
	m_cookie_length = t->Length();

	if (m_cookie_data) delete [] m_cookie_data;
	m_cookie_data = new unsigned char[m_cookie_length];

	memcpy(m_cookie_data, t->Value(), m_cookie_length);

	SignalLog(LogEvent::INFO, "Authorisation accepted");
	
	DisconnectAuthorizer();
	ConnectBOS();

      } else {
	// Problemo
	DisconnectedEvent::Reason st;

	if (tlvlist.exists(TLV_ErrorCode)) {
	  ErrorCodeTLV *t = static_cast<ErrorCodeTLV*>(tlvlist[TLV_ErrorCode]);
	  ostringstream ostr;
	  ostr << "Error logging in Error Code: " << t->Value();
	  SignalLog(LogEvent::ERROR, ostr.str());
	  switch(t->Value()) {
	  case 0x01:
	    st = DisconnectedEvent::FAILED_BADUSERNAME;
	    break;
	  case 0x02:
	    st = DisconnectedEvent::FAILED_TURBOING;
	    break;
	  case 0x03:
	    st = DisconnectedEvent::FAILED_BADPASSWORD;
	    break;
	  case 0x05:
	    st = DisconnectedEvent::FAILED_MISMATCH_PASSWD;
	    break;
	  case 0x18:
	    st = DisconnectedEvent::FAILED_TURBOING;
	    break;
	  default:
	    st = DisconnectedEvent::FAILED_UNKNOWN;
	  }
	} else if (m_state == AUTH_AWAITING_AUTH_REPLY) {
	    SignalLog(LogEvent::ERROR, "Error logging in, no error code given(?!)");
	    st = DisconnectedEvent::FAILED_UNKNOWN;
	} else {
	  st = DisconnectedEvent::REQUESTED;
	}
	DisconnectAuthorizer();
	SignalDisconnect(st); // signal client (error)
      }

    } else {

      TLVList tlvlist;
      tlvlist.Parse(b, TLV_ParseMode_Channel04, (short unsigned int)-1);

      DisconnectedEvent::Reason st;
      
      if (tlvlist.exists(TLV_DisconnectReason)) {
	DisconnectReasonTLV *t = static_cast<DisconnectReasonTLV*>(tlvlist[TLV_DisconnectReason]);
	  switch(t->Value()) {
	  case 0x0001:
	    st = DisconnectedEvent::FAILED_DUALLOGIN;
	    break;
	  default:
	    st = DisconnectedEvent::FAILED_UNKNOWN;
	  }

	} else {
	  SignalLog(LogEvent::WARN, "Unknown packet received on channel 4, disconnecting");
	  st = DisconnectedEvent::FAILED_UNKNOWN;
	}
	DisconnectBOS();
	SignalDisconnect(st); // signal client (error)
    }

  }

  // -----------------------------------------------------

  /**
   *  Perform any regular time dependant tasks.
   *
   *  Poll must be called regularly (at least every 60 seconds) but I
   *  recommended 5 seconds, so timeouts work with good granularity.
   *  It is not related to the socket callback and socket listening.
   *  The client must call this Poll fairly regularly, to ensure that
   *  timeouts on message sending works correctly, and that the server
   *  is pinged once every 60 seconds.
   */
  void Client::Poll() {
    time_t now = time(NULL);
    if (now > m_last_server_ping + 60) {
      PingServer();
      m_last_server_ping = now;

    }

    m_reqidcache.clearoutPoll();
    m_cookiecache.clearoutPoll();
  }


  /**
   *  Register a new UIN.
   */
  void Client::RegisterUIN() {
    if (m_state == NOT_CONNECTED)
      ConnectAuthorizer(UIN_AWAITING_CONN_ACK);
  }

  /**
   *  Boolean to determine if you are connected or not.
   *
   * @return connected
   */
  bool Client::isConnected() const {
    return (m_state == BOS_LOGGED_IN);
  }

  void Client::HandleUserInfoSNAC(UserInfoSNAC *snac) {
    // this should only be personal info
    const UserInfoBlock &ub = snac->getUserInfo();
    if (ub.getUIN() == m_self->getUIN()) {
      // currently only interested in our external IP
      // - we might be behind NAT
      if (ub.getExtIP() != 0) m_ext_ip = ub.getExtIP();

      // Check for status change
      Status newstat = Contact::MapICQStatusToStatus( ub.getStatus() );
      bool newinvis = Contact::MapICQStatusToInvisible( ub.getStatus() );
      m_self->setStatus(newstat, newinvis);
    }
  }

  /**
   *  Used for sending a message event from the client.  The Client
   *  should create the specific MessageEvent by dynamically
   *  allocating it with new. The library will take care of deleting
   *  it when appropriate. The MessageEvent will persist whilst the
   *  message has not be confirmed as delivered or failed yet. Exactly
   *  the same MessageEvent is signalled back in the messageack signal
   *  callback, so a client could use pointer equality comparison to
   *  match messages it has sent up to their acks.
   */
  void Client::SendEvent(MessageEvent *ev) {
    switch (ev->getType()) {

      case MessageEvent::Normal:
      case MessageEvent::URL:
      case MessageEvent::AwayMessage:
      if (!SendDirect(ev)) SendViaServer(ev);
	break;

      case MessageEvent::Email:
	//	m_smtp.SendEvent(ev);
	SignalLog(LogEvent::WARN,"Unable to send Email");
	delete ev;
	break;

    default:
      SendViaServer(ev);
      break;
    }
  }
  
  bool Client::SendDirect(MessageEvent *ev) {
    ContactRef c = ev->getContact();
    if (!c->getDirect()) return false;
    /*    DirectClient *dc = ConnectDirect(c);
	  if (dc == NULL) return false;
	  dc->SendEvent(ev);
	  return true;
    */
    return false;
  }


  void Client::SendViaServer(MessageEvent *ev) {
    ContactRef c = ev->getContact();

    if (m_self->getStatus() == STATUS_OFFLINE) {
      ev->setFinished(true);
      ev->setDelivered(false);
      ev->setDirect(false);
      ev->setDeliveryFailureReason(MessageEvent::Failed_NotConnected);
      SignalMessageAck(ev);
      //      messageack.emit(ev);
      delete ev;
      return;
    }

    if (ev->getType() == MessageEvent::Normal
	|| ev->getType() == MessageEvent::URL) {
      
      /*
       * Normal messages and URL messages sent via the server
       * can be sent as advanced for ICQ2000 users online, in which
       * case they can be ACKed, otherwise there is no way of
       * knowing if it's received
       */
      
      if (c->get_accept_adv_msgs())
	SendViaServerAdvanced(ev);
      else {
	SendViaServerNormal(ev);
	delete ev;
      }
      
    } else if (ev->getType() == MessageEvent::AwayMessage) {

      /*
       * Away message requests sent via the server only
       * work for ICQ2000 clients online, otherwise there
       * is no way of getting the away message, so we signal the ack
       * to the client as non-delivered
       */

      if (c->get_accept_adv_msgs())
	SendViaServerAdvanced(ev);
      else {
	ev->setFinished(true);
	ev->setDelivered(false);
	ev->setDirect(false);
	ev->setDeliveryFailureReason(MessageEvent::Failed_ClientNotCapable);
	SignalMessageAck(ev);
	//	messageack.emit(ev);
	delete ev;
      }
      
    } else if (ev->getType() == MessageEvent::AuthReq
	       || ev->getType() == MessageEvent::AuthAck
	       || ev->getType() == MessageEvent::UserAdd) {
      
      /*
       * This seems the sure way of sending authorisation messages and
       * user added me notices. They can't be sent direct.
       */
      SendViaServerNormal(ev);
      delete ev;

    } else if (ev->getType() == MessageEvent::SMS) {

      /*
       * SMS Messages are sent via a completely different mechanism.
       *
       */
      SMSMessageEvent *sv = static_cast<SMSMessageEvent*>(ev);
      SrvSendSNAC ssnac(sv->getMessage(), c->getNormalisedMobileNo(), m_self->getUIN(), "", sv->getRcpt());

      unsigned int reqid = NextRequestID();
      m_reqidcache.insert( reqid, new SMSEventCacheValue( sv ) );
      ssnac.setRequestID( reqid );

      FLAPwrapSNACandSend( ssnac );

    }

  }

  void Client::SendViaServerAdvanced(MessageEvent *ev) 
  {
    ContactRef c = ev->getContact();
    UINICQSubType *ist = m_message_handler.handleOutgoing(ev);
    ist->setAdvanced(true);
    
    MsgSendSNAC msnac(ist);
    msnac.setAdvanced(true);
    msnac.setSeqNum( c->nextSeqNum() );
    ICBMCookie ck = m_cookiecache.generateUnique();
    msnac.setICBMCookie( ck );
    m_cookiecache.insert( ck, ev );

    msnac.set_capabilities( c->get_capabilities() );
    
    FLAPwrapSNACandSend( msnac );
    
    delete ist;
  }
  
  void Client::SendViaServerNormal(MessageEvent *ev)
  {
    ContactRef c = ev->getContact();
    UINICQSubType *ist = m_message_handler.handleOutgoing(ev);
    ist->setAdvanced(false);
    
    MsgSendSNAC msnac(ist);
    msnac.setAdvanced(false);

    FLAPwrapSNACandSend( msnac );
    
    ev->setFinished(true);
    ev->setDelivered(true);
    ev->setDirect(false);
    ICQMessageEvent *cev;
    if ((cev = dynamic_cast<ICQMessageEvent*>(ev)) != NULL) cev->setOfflineMessage(true);
    
    SignalMessageAck(ev);
    //    messageack.emit(ev);
    
    /* XXX WP */
    delete ist;
  }
  
  void Client::PingServer() {
    Buffer b(&m_translator);
    Buffer::marker mk = FLAPHeader(b,0x05);
    FLAPFooter(b,mk);
    Send(b);
  }


  void Client::uploadSelfDetails()
  {
    Buffer b(&m_translator);
    
    FLAPwrapSNAC( b, SrvUpdateMainHomeInfo(m_self->getUIN(), m_self->getMainHomeInfo()) );
    FLAPwrapSNAC( b, SrvUpdateWorkInfo(m_self->getUIN(), m_self->getWorkInfo()) );
    FLAPwrapSNAC( b, SrvUpdateHomepageInfo(m_self->getUIN(), m_self->getHomepageInfo()) );
    FLAPwrapSNAC( b, SrvUpdateAboutInfo(m_self->getUIN(), m_self->getAboutInfo()) );
    
    Send(b);
  }
    
  /**
   *  Set your status. This is used to set your status, as well as to
   *  connect and disconnect from the network. When you wish to
   *  connect to the ICQ network, set status to something other than
   *  STATUS_OFFLINE and connecting will be initiated. When you wish
   *  to disconnect set the status to STATUS_OFFLINE and disconnection
   *  will be initiated.
   *
   * @param st the status
   * @param inv whether to be invisible or not
   */
  void Client::setStatus(const Status st, bool inv)
  {
    m_status_wanted = st;
    m_invisible_wanted = inv;

    if (st == STATUS_OFFLINE) {
      if (m_state != NOT_CONNECTED)
	Disconnect(DisconnectedEvent::REQUESTED);

      return;
    }

    if (m_state == BOS_LOGGED_IN) {
      /*
       * The correct sequence of events are:
       * - when going from visible to invisible
       *   - send the Add to Visible list (or better named Set in this case)
       *   - send set Status
       * - when going from invisible to visible
       *   - send set Status
       *   - send the Add to Invisible list (or better named Set in this case)
       */

      Buffer b(&m_translator);

      if (!m_self->isInvisible() && inv) {
	// visible -> invisible
	FLAPwrapSNAC( b, AddVisibleSNAC(m_visible_list) );
      }
	
      FLAPwrapSNAC( b, SetStatusSNAC(Contact::MapStatusToICQStatus(st, inv), m_web_aware) );
      
      if (m_self->isInvisible() && !inv) {
	// invisible -> visible
	FLAPwrapSNAC( b, AddInvisibleSNAC(m_invisible_list) );
      }
      
      Send(b);

    } else {
      // We'll set this as the initial status upon connecting
      m_status_wanted = st;
      m_invisible_wanted = inv;
      
      // start connecting if not already
      if (m_state == NOT_CONNECTED)
	ConnectAuthorizer(AUTH_AWAITING_CONN_ACK);
    }
  }

  /**
   *  Set your status, without effecting invisibility (your last
   *  requested invisible state will be used).
   *
   * @param st the status
   */
  void Client::setStatus(const Status st)
  {
    setStatus(st, m_invisible_wanted);
  }

  /**
   *  Set your invisibility, without effecting status (your last
   *  requested status will be used).
   *
   * @param inv invisibility
   */
  void Client::setInvisible(bool inv)
  {
    setStatus(m_status_wanted, inv);
  }

  void Client::setWebAware(bool wa)
  {
    if (m_web_aware!=wa)
    {
      m_web_aware = wa;
      if (m_self->getStatus() != STATUS_OFFLINE)
      setStatus(m_status_wanted, m_invisible_wanted);
    }
  }

  /**
   *  Get your current status.
   *
   * @return your current status
   */
  Status Client::getStatus() const {
    return m_self->getStatus();
  }

  /**
   *  Get your invisible status
   * @return Invisible boolean
   *
   */
  bool Client::getInvisible() const
  {
    return m_self->isInvisible();
  }

  /**
   *  Get the last requested status
   */
  Status Client::getStatusWanted() const 
  {
    return m_status_wanted;
  }
  
  /**
   *  Get the last invisibility status wanted
   */
  bool Client::getInvisibleWanted() const
  {
    return m_invisible_wanted;
  }

  bool Client::getWebAware() const
  {
    return m_web_aware;
  }

  void Client::contactlist_cb(ContactListEvent *ev)
  {
    ContactRef c = ev->getContact();
    
    if (ev->getType() == ContactListEvent::UserAdded) {
      
      if (c->isICQContact() && m_state == BOS_LOGGED_IN) {
		FLAPwrapSNACandSend( AddBuddySNAC(c) );
		
		// fetch detailed userinfo from server
		fetchDetailContactInfo(c);
      }

    } else if (ev->getType() == ContactListEvent::UserRemoved) {
      
      if (c->isICQContact() && m_state == BOS_LOGGED_IN) {
	FLAPwrapSNACandSend( RemoveBuddySNAC(c) );
      }
      
    }
    
    // re-emit on the Client signal
    SignalContactList(ev);
    //    contactlist.emit(ev);
    
  }
  

  void Client::visiblelist_cb(ContactListEvent *ev)
  {
    ContactRef c = ev->getContact();
    
    if (ev->getType() == ContactListEvent::UserAdded) {
      
      if (c->isICQContact() && m_state == BOS_LOGGED_IN && m_self->isInvisible()) {
	FLAPwrapSNACandSend( AddVisibleSNAC(c) );

      }

    } else {

      if (c->isICQContact() && m_state == BOS_LOGGED_IN && m_self->isInvisible()) {
	FLAPwrapSNACandSend( RemoveVisibleSNAC(c) );
      }

    }
    
  }
  

  void Client::invisiblelist_cb(ContactListEvent *ev)
  {
    ContactRef c = ev->getContact();
    
    if (ev->getType() == ContactListEvent::UserAdded) {
      
      if (c->isICQContact() && m_state == BOS_LOGGED_IN && !m_self->isInvisible()) {
	FLAPwrapSNACandSend( AddInvisibleSNAC(c) );

      }

    } else {

      if (c->isICQContact() && m_state == BOS_LOGGED_IN && !m_self->isInvisible()) {
	FLAPwrapSNACandSend( RemoveInvisibleSNAC(c) );
      }

    }
    
  }
  

  /**
   *  Add a contact to your list.
   *
   * @param c the contact passed as a reference counted object (ref_ptr<Contact> or ContactRef).
   */
  void Client::addContact(ContactRef c) {

    if (!m_contact_list.exists(c->getUIN())) {
      c->setClient(this);
      m_contact_list.add(c);
      //      c->status_change_signal.connect( contact_status_change_signal.slot() );
      //      c->userinfo_change_signal.connect( contact_userinfo_change_signal.slot() );
    }

  }

  /**
   *  Remove a contact from your list.
   *
   * @param uin the uin of the contact to be removed
   */
  void Client::removeContact(const unsigned int uin) {
    if (m_contact_list.exists(uin)) {
      m_contact_list.remove(uin);
    }
  }
  
  /**
   *  Add a contact to your visible list.
   *
   * @param c the contact passed as a reference counted object (ref_ptr<Contact> or ContactRef).
   */
  void Client::addVisible(ContactRef c) {

    if (!m_visible_list.exists(c->getUIN())) {
      m_visible_list.add(c);
    }

  }

  /**
   *  Remove a contact from your visible list.
   *
   * @param uin the uin of the contact to be removed
   */
  void Client::removeVisible(const unsigned int uin) {
    if (m_visible_list.exists(uin)) {
      m_visible_list.remove(uin);
    }
  }
  
  /**
   *  Add a contact to your invisible list.
   *
   * @param c the contact passed as a reference counted object (ref_ptr<Contact> or ContactRef).
   */
  void Client::addInvisible(ContactRef c) {

    if (!m_invisible_list.exists(c->getUIN())) {
      m_invisible_list.add(c);
    }

  }

  /**
   *  Remove a contact from your invisible list.
   *
   * @param uin the uin of the contact to be removed
   */
  void Client::removeInvisible(const unsigned int uin) {
    if (m_invisible_list.exists(uin)) {
      m_invisible_list.remove(uin);
    }
  }
  
  void Client::SignalServerBasedContactList(const ContactList& l) {
    ServerBasedContactEvent ev(l);
    SignalServerContactEvent(&ev);
    //    server_based_contact_list.emit(&ev);
  }

  ContactRef Client::getSelfContact() { return m_self; }

  /**
   *  Get the Contact object for a given uin.
   *
   * @param uin the uin
   * @return a pointer to the Contact object. NULL if no Contact with
   * that uin exists on your list.
   */
  ContactRef Client::getContact(const unsigned int uin) {
    if (m_contact_list.exists(uin)) {
      return m_contact_list[uin];
    } else {
      return NULL;
    }
  }

  /**
   *  Get the ContactList object used for the main library.
   *
   * @return a reference to the ContactList
   */
  ContactList& Client::getContactList()
  {
    return m_contact_list;
  }

  /**
   *  Request the simple contact information for a Contact.  This
   *  consists of the contact alias, firstname, lastname and
   *  email. When the server has replied with the details the library
   *  will signal a user info changed for this contact.
   *
   * @param c contact to fetch info for
   * @see ContactListEvent
   */
  void Client::fetchSimpleContactInfo(ContactRef c) {
    Buffer b(&m_translator);

    if ( !c->isICQContact() ) return;

    SignalLog(LogEvent::INFO, "Sending request Simple Userinfo Request");
    FLAPwrapSNACandSend( SrvRequestSimpleUserInfo( m_self->getUIN(), c->getUIN() ) );
  }

  /**
   *  Request the detailed contact information for a Contact. When the
   *  server has replied with the details the library will signal a
   *  user info changed for this contact.
   *
   * @param c contact to fetch info for
   * @see ContactListEvent
   */
  void Client::fetchDetailContactInfo(ContactRef c) {
    if ( !c->isICQContact() ) return;

    SignalLog(LogEvent::INFO, "Sending request Detailed Userinfo Request");

    unsigned int reqid = NextRequestID();
    m_reqidcache.insert( reqid, new UserInfoCacheValue(c) );
    SrvRequestDetailUserInfo ssnac( m_self->getUIN(), c->getUIN() );
    ssnac.setRequestID( reqid );
    FLAPwrapSNACandSend( ssnac );
  }

  void Client::fetchSelfSimpleContactInfo()
  {
    fetchSimpleContactInfo(m_self);
  }

  void Client::fetchSelfDetailContactInfo()
  {
    fetchDetailContactInfo(m_self);
  }

  void Client::fetchServerBasedContactList() {
    SignalLog(LogEvent::INFO, "Requesting Server-based contact list");
    FLAPwrapSNACandSend( RequestSBLSNAC() );
  }

  SearchResultEvent* Client::searchForContacts
    (const string& nickname, const string& firstname,
     const string& lastname)
  {
    SearchResultEvent *ev = new SearchResultEvent( SearchResultEvent::ShortWhitepage );

    unsigned int reqid = NextRequestID();
    m_reqidcache.insert( reqid, new SearchCacheValue( ev ) );

    SrvRequestShortWP ssnac( m_self->getUIN(), nickname, firstname, lastname );
    ssnac.setRequestID( reqid );

    SignalLog(LogEvent::INFO, "Sending short whitepage search");
    FLAPwrapSNACandSend( ssnac );

    return ev;
  }

  SearchResultEvent* Client::searchForContacts
    (const string& nickname, const string& firstname,
     const string& lastname, const string& email,
     AgeRange age, Sex sex, unsigned char language, const string& city,
     const string& state, unsigned short country,
     const string& company_name, const string& department,
     const string& position, bool only_online)
  {
    SearchResultEvent *ev = new SearchResultEvent( SearchResultEvent::FullWhitepage );

    unsigned int reqid = NextRequestID();
    m_reqidcache.insert( reqid, new SearchCacheValue( ev ) );

    unsigned short min_age, max_age;

    switch(age) {
	case range_18_22:
	    min_age = 18;
	    max_age = 22;
	    break;
	case range_23_29:
	    min_age = 23;
	    max_age = 29;
	    break;
	case range_30_39:
	    min_age = 30;
	    max_age = 39;
	    break;
	case range_40_49:
	    min_age = 40;
	    max_age = 49;
	    break;
	case range_50_59:
	    min_age = 50;
	    max_age = 59;
	    break;
	case range_60_above:
	    min_age = 60;
	    max_age = 0x2710;
	    break;
	default:
	    min_age = max_age = 0;
	    break;
    }

    SrvRequestFullWP ssnac( m_self->getUIN(), nickname, firstname, lastname, email,
			    min_age, max_age, (unsigned char)sex, language, city, state,
			    country, company_name, department, position,
			    only_online);
    ssnac.setRequestID( reqid );

    SignalLog(LogEvent::INFO, "Sending full whitepage search");
    FLAPwrapSNACandSend( ssnac );

    return ev;
  }

  SearchResultEvent* Client::searchForContacts(unsigned int uin)
  {
    SearchResultEvent *ev = new SearchResultEvent( SearchResultEvent::UIN );

    unsigned int reqid = NextRequestID();
    m_reqidcache.insert( reqid, new SearchCacheValue( ev ) );

    SrvRequestSimpleUserInfo ssnac( m_self->getUIN(), uin );
    ssnac.setRequestID( reqid );

    SignalLog(LogEvent::INFO, "Sending simple user info request");
    FLAPwrapSNACandSend( ssnac );

    return ev;
  }

  SearchResultEvent* Client::searchForContacts(const std::string& keyword)
  {
    SearchResultEvent *ev = new SearchResultEvent( SearchResultEvent::Keyword );
    unsigned int reqid = NextRequestID();
    m_reqidcache.insert( reqid, new SearchCacheValue( ev ) );
    
    SrvRequestKeywordSearch ssnac( m_self->getUIN(), keyword );
    ssnac.setRequestID( reqid );
    
    SignalLog(LogEvent::INFO, "Sending contact keyword search request");
    FLAPwrapSNACandSend( ssnac );
    
    return ev;
  }

  void Client::Disconnect(DisconnectedEvent::Reason r) {
    if (m_state == NOT_CONNECTED) {
      SignalDisconnect(r);
      return;
    }
    
    SignalLog(LogEvent::INFO, "Client disconnecting");

    if (m_state == AUTH_AWAITING_CONN_ACK || m_state == AUTH_AWAITING_AUTH_REPLY
	|| m_state == UIN_AWAITING_CONN_ACK || m_state == UIN_AWAITING_UIN_REPLY) {
      DisconnectAuthorizer();
    } else {
      DisconnectBOS();
    }

    SignalDisconnect(r);
  }

  /**
   *  Set the translation map file to use for character set translation.
   * @param szMapFileName the name of the translation map file
   * @return whether setting translation map was a success
   */
  bool Client::setTranslationMap(const string& szMapFileName) { 
    try{
      m_translator.setTranslationMap(szMapFileName);
    } catch (TranslatorException e) {
      SignalLog(LogEvent::WARN, e.what());
      return false; 
    }
    return true;
  }

  /**
   *  Get the File name of the translation map currently in use.
   * @return filename of translation map
   */
  const string& Client::getTranslationMapFileName() const {
    return m_translator.getMapFileName();
  }

  /**
   *  Get the Name of the translation map currently in use.
   * @return name of translation map
   */
  const string& Client::getTranslationMapName() const {
    return m_translator.getMapName();
  }

  /**
   *  Determine whether the default map (no translation) is in use.
   * @return whether using the default map
   */
  bool Client::usingDefaultMap() const {
    return m_translator.usingDefaultMap();
  }

  /**
   *  Get your uin.
   * @return your UIN
   */
  unsigned int Client::getUIN() const 
  {
    return m_self->getUIN();
  }

    /**
     *  Set your uin.
     *  Use to set what the uin you would like to log in as, before connecting.
     * @param uin your UIN
     */
  void Client::setUIN(unsigned int uin)
  {
    m_self->setUIN(uin);
  }

  /** 
   *  Set the password to use at login.
   * @param password your password
   */
  void Client::setPassword(const string& password) 
  {
    m_password = password;
  }

  /**
   *  Get the password you set for login
   * @return your password
   */
  string Client::getPassword() const
  {
    return m_password;
  }
  
  /**
   *  set the hostname of the login server.
   *  You needn't touch this normally, it will default automatically to login.icq.com.
   *
   * @param host The host name of the server
   */
  void Client::setLoginServerHost(const string& host) {
    m_authorizerHostname = host;
  }

  /**
   *  get the hostname for the currently set login server.
   *
   * @return the hostname
   */
  string Client::getLoginServerHost() const {
    return m_authorizerHostname;
  }
  
  /**
   *  set the port on the login server to connect to
   *
   * @param port the port number
   */
  void Client::setLoginServerPort(const unsigned short& port) {
    m_authorizerPort = port;
  }

  /**
   *  get the currently set port on the login server.
   *
   * @return the port number
   */
  unsigned short Client::getLoginServerPort() const {
    return m_authorizerPort;
  }
  
  /**
   *  set whether to override the port used to connect to the BOS
   *  server.  If you would like to ignore the port that the login
   *  server tells you to connect to on the BOS server and instead use
   *  your own, set this to true and call setBOSServerPort with the
   *  port you would like to use. This method is largely unnecessary,
   *  if you set a different login port - for example to get through
   *  firewalls that block 5190, the login server will accept it fine
   *  and in the redirect message doesn't specify a port, so the
   *  library will default to using the same one as it used to connect
   *  to the login server anyway.
   *
   * @param b override redirect port
   */
  void Client::setBOSServerOverridePort(const bool& b) {
    m_bosOverridePort = b;
  }

  /**
   *  get whether the BOS redirect port will be overridden.
   *
   * @return override redirect port
   */
  bool Client::getBOSServerOverridePort() const {
    return m_bosOverridePort;
  }
  
  /**
   *  set the port to use to connect to the BOS server. This will only
   *  be used if you also called setBOSServerOverridePort(true).
   *
   * @param port the port number
   */
  void Client::setBOSServerPort(const unsigned short& port) {
    m_bosPort = port;
  }

  /**
   *  get the port that will be used on the BOS server.
   *
   * @return the port number
   */
  unsigned short Client::getBOSServerPort() const {
    return m_bosPort;
  }
  
  /** 
   *  set the upper bound of the portrange for incoming connections (esp. behind a firewall)
   *  you have to restart the TCPServer(s) for this to take effect
   *  
   * @param upper upper bound
   */
  void Client::setPortRangeUpperBound(unsigned short upper) {
    m_upper_port=upper;
  }

  /** 
   *  set the lower bound of the portrange for incoming connections (esp. behind a firewall)
   *  you have to restart the TCPServer(s) for this to take effect
   *  
   * @param lower lower bound
   */
  void Client::setPortRangeLowerBound(unsigned short lower) {
    m_lower_port=lower;
  }

  /**
   *  get upper bound of the portrange used for incoming connections
   *
   * @return upper bound
   */
  unsigned short Client::getPortRangeUpperBound() const
  {
    return m_upper_port;
  }

  /**
   *  get lower bound of the portrange used for incoming connections
   *
   * @return lower bound
   */
  unsigned short Client::getPortRangeLowerBound() const 
  {
    return m_lower_port;
  }

  /**
   *  set whether a portrange should be used for incoming connections
   *
   * @param b whether to use a portrange
   */ 
  void Client::setUsePortRange(bool b) {
    m_use_portrange=b;
  }
  
  /**
   *  get whether a portrange should be used for incoming connections
   *
   * @return whether to use a portrange
   */ 
  bool Client::getUsePortRange() const 
  {
    return m_use_portrange;
  }

}

