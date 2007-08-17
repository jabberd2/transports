/**
 * libICQ2000 Client header
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

#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <string>

#include <map>

#include <stdio.h>

#include <time.h>

#include <libicq2000/buffer.h>
#include <libicq2000/events.h>
#include <libicq2000/constants.h>
#include <libicq2000/Contact.h>
#include <libicq2000/ContactList.h>
#include <libicq2000/DirectClient.h>
#include <libicq2000/DCCache.h>
#include <libicq2000/Translator.h>
#include <libicq2000/RequestIDCache.h>
#include <libicq2000/ICBMCookieCache.h>
#include <libicq2000/userinfoconstants.h>
#include <libicq2000/MessageHandler.h>

using std::string;

namespace ICQ2000 {
  
  // declare some SNAC classes - vastly decreases header dependancies
  class MessageSNAC;
  class MessageACKSNAC;
  class MessageOfflineUserSNAC;
  class SrvResponseSNAC;
  class UINResponseSNAC;
  class RateInfoChangeSNAC;
  class BuddyOnlineSNAC;
  class BuddyOfflineSNAC;
  class UserInfoSNAC;
  class OutSNAC;

  /**
   *  The main library object.  This is the object the user interface
   *  instantiates for a connection, hooks up to signal on and has the
   *  methods to connect, disconnect and send events from.
   */
  class Client {
   private:
    enum State { NOT_CONNECTED,
		 AUTH_AWAITING_CONN_ACK,
		 AUTH_AWAITING_AUTH_REPLY,
		 BOS_AWAITING_CONN_ACK,
		 BOS_AWAITING_LOGIN_REPLY,
		 BOS_LOGGED_IN,
		 UIN_AWAITING_CONN_ACK,
		 UIN_AWAITING_UIN_REPLY
    } m_state;

    ContactRef m_self;
    string m_password;
    Status m_status_wanted;
    bool m_invisible_wanted;
    bool m_web_aware;

    string m_authorizerHostname;
    unsigned short m_authorizerPort;

    string m_bosHostname;
    unsigned short m_bosPort;
    bool m_bosOverridePort;

    unsigned short m_client_seq_num;
    unsigned int m_requestid;
    
    Translator m_translator;

    ContactList m_contact_list;
    
    ContactList m_visible_list;
    ContactList m_invisible_list;

    MessageHandler m_message_handler;

    unsigned char *m_cookie_data;
    unsigned short m_cookie_length;

    unsigned int m_ext_ip;
    bool m_use_portrange;
    unsigned short m_upper_port, m_lower_port;

    time_t m_last_server_ping;

    RequestIDCache m_reqidcache;
    ICBMCookieCache m_cookiecache;

    Buffer m_recv;
   
    void Init();
    unsigned short NextSeqNum();
    unsigned int NextRequestID();

    void ConnectAuthorizer(State state);
    void DisconnectAuthorizer();
    void ConnectBOS();
    void DisconnectBOS();

    // -- Ping server --
    void PingServer();

    // ------------------ Signal dispatchers -----------------
    void SignalConnect();
    void SignalDisconnect(DisconnectedEvent::Reason r);
    void SignalMessage(MessageSNAC *snac);
    void SignalMessageACK(MessageACKSNAC *snac);
    void SignalMessageOfflineUser(MessageOfflineUserSNAC *snac);
    void SignalSrvResponse(SrvResponseSNAC *snac);
    void SignalUINResponse(UINResponseSNAC *snac);
    void SignalUINRequestError();
    void SignalRateInfoChange(RateInfoChangeSNAC *snac);
    void SignalLog(LogEvent::LogType type, const string& msg);
    void SignalUserOnline(BuddyOnlineSNAC *snac);
    void SignalUserOffline(BuddyOfflineSNAC *snac);
    void SignalServerBasedContactList(const ContactList& l);
    // ------------------ Outgoing packets -------------------

    // -------------- Callbacks from Contacts ----------------

    void SendAuthReq();
    void SendNewUINReq();
    void SendCookie();
    void SendCapabilities();
    void SendRateInfoRequest();
    void SendRateInfoAck();
    void SendPersonalInfoRequest();
    void SendAddICBMParameter();
    void SendSetUserInfo();
    void SendLogin();
    void SendOfflineMessagesRequest();
    void SendOfflineMessagesACK();

    void SendAdvancedACK(MessageSNAC *snac);

    virtual void Send(Buffer& b)
      { //fprintf(stderr,"virtual method");    
      }

    void HandleUserInfoSNAC(UserInfoSNAC *snac);

    Buffer::marker FLAPHeader(Buffer& b, unsigned char channel);
    void FLAPFooter(Buffer& b, Buffer::marker& mk);

    void FLAPwrapSNAC(Buffer& b, const OutSNAC& snac);
    void FLAPwrapSNACandSend(const OutSNAC& snac);

    // ------------------ Incoming packets -------------------

    /**
     *  non-blocking receives all waiting packets from server
     *  and parses and handles them
     */
    //    void RecvFromServer();

    void Parse();
    void ParseCh1(Buffer& b, unsigned short seq_num);
    void ParseCh2(Buffer& b, unsigned short seq_num);
    void ParseCh3(Buffer& b, unsigned short seq_num);
    void ParseCh4(Buffer& b, unsigned short seq_num);

    // -------------------------------------------------------

    ContactRef getUserInfoCacheContact(unsigned int reqid);

    bool SendDirect(MessageEvent *ev);

    void SendViaServer(MessageEvent *ev);
    void SendViaServerAdvanced(MessageEvent *ev);
    void SendViaServerNormal(MessageEvent *ev);
    

   public:
    Client();
    Client(const unsigned int uin, const string& password);
    virtual ~Client();

    /* WP changes */
    void Disconnect(DisconnectedEvent::Reason r = DisconnectedEvent::REQUESTED);

    void ICBMCookieCache_expired_cb(MessageEvent *ev);
    void reqidcache_expired_cb(	RequestIDCacheValue *v );
    void dc_log_cb(LogEvent *ev);
    void dc_messageack_cb(MessageEvent *ev);

    // -------------- Callbacks from ContactList -------------
    void contactlist_cb(ContactListEvent *ev);

    // ------- Callbacks from visible, invisible lists -------
    void visiblelist_cb(ContactListEvent *ev);
    void invisiblelist_cb(ContactListEvent *ev);


    void RecvFromServer(const unsigned char *buf, int len);

    int isCookieData() {
      if (m_cookie_data)
	return 1;

      return 0; /* no cookie */
    }

    /* sockets */
    virtual void SocketDisconnect() 
      { //fprintf(stderr,"virtual method");    
      }
    virtual void SocketConnect(const char* host,int port,int type)
      {// fprintf(stderr,"virtual method");    
      }

    /* signals */

    //connected
    virtual void SignalConnected(ConnectedEvent *ev)
      {// fprintf(stderr,"virtual method 1");    
      }
    //disconnected
    virtual void SignalDisconnected(DisconnectedEvent *ev)
      { //fprintf(stderr,"virtual method 2");    
      }
    //messaged
    virtual void SignalMessaged(MessageEvent *ev)  { 
      //fprintf(stderr,"virtual method 3");   
    }
    //massage_ack
    virtual void SignalMessageAck(MessageEvent *ev)
      {// fprintf(stderr,"virtual method 4");    
      }
    //contactlist
    virtual void SignalContactList(ContactListEvent *ev)
      { //fprintf(stderr,"virtual method 5");    
      }
    //want_auto_resp
    virtual void SignalAwayMessageEvent(ICQMessageEvent* ev)
      { //fprintf(stderr,"virtual method 7");   
      }
    //serach_result
    virtual void SignalSearchResultEvent(SearchResultEvent *ev)
      {// fprintf(stderr,"virtual method 8");    
      }
    //newuin
    virtual void SignalNewUIN(NewUINEvent *ev)
      { //fprintf(stderr,"virtual method 9");    
      }
    //rate
    virtual void SignalRateEvent(RateInfoChangeEvent *ev)
      {  }
    //logger
    virtual void SignalLogE(LogEvent *ev)
      {// fprintf(stderr,"virtual method 10");    
      }

    //server_based_contact_list
    virtual void SignalServerContactEvent(ServerBasedContactEvent* ev) 
      {// fprintf(stderr,"virtual method 11"); 
      }

    // contact_userinfo_change_signal
    virtual void SignalUserInfoChangeEvent(UserInfoChangeEvent *ev)
      { //fprintf(stderr,"virtual method 12"); 
      }

    // contact_status_change_signal
    virtual void SignalStatusChangeEvent(StatusChangeEvent *ev)
      { //fprintf(stderr,"virtual method 12"); 
      }
    /*WP*/


    void setUIN(unsigned int uin);
    unsigned int getUIN() const;
    void setPassword(const string& password);
    string getPassword() const;

    ContactRef getSelfContact();

    bool setTranslationMap(const string& szMapFileName);
    const string& getTranslationMapFileName() const;
    const string& getTranslationMapName() const;
    bool usingDefaultMap() const;

    /* * SigC::Signal1<void,ConnectedEvent*> connected;
     *SigC::Signal1<void,DisconnectedEvent*> disconnected;
     *SigC::Signal1<void,MessageEvent*> messaged;
     *SigC::Signal1<void,MessageEvent*> messageack;
	SigC::Signal1<void,ContactListEvent*> contactlist;
	SigC::Signal1<void,UserInfoChangeEvent*> contact_userinfo_change_signal;
	SigC::Signal1<void,StatusChangeEvent*> contact_status_change_signal;
	*SigC::Signal1<void,NewUINEvent*> newuin;
	SigC::Signal1<void,RateInfoChangeEvent*> rate;
	SigC::Signal1<void,LogEvent*> logger;
	SigC::Signal1<void,UserInfoChangeEvent*> self_contact_userinfo_change_signal;
	SigC::Signal1<void,StatusChangeEvent*> self_contact_status_change_signal;
	SigC::Signal1<void,ICQMessageEvent*> want_auto_resp;
	*SigC::Signal1<void,SearchResultEvent*> search_result;
	SigC::Signal1<void,ServerBasedContactEvent*> server_based_contact_list;
    */
    // -------------

    // -- Send calls --
    void SendEvent(MessageEvent *ev);

    // -- Set Status --
    void setStatus(const Status st);
    void setStatus(const Status st, bool inv);
    void setInvisible(bool inv);
    void setWebAware(bool wa);
    
    Status getStatus() const;
    bool getInvisible() const;

    Status getStatusWanted() const;
    bool getInvisibleWanted() const;

    bool getWebAware() const;

    void uploadSelfDetails();
    
    // -- Contact List --
    void addContact(ContactRef c);
    void removeContact(const unsigned int uin);
    void addVisible(ContactRef c);
    void removeVisible(const unsigned int uin);
    void addInvisible(ContactRef c);
    void removeInvisible(const unsigned int uin);
    ContactRef getContact(const unsigned int uin);

    ContactList& getContactList();

    void fetchSimpleContactInfo(ContactRef c);
    void fetchDetailContactInfo(ContactRef c);
    void fetchServerBasedContactList();
    void fetchSelfSimpleContactInfo();
    void fetchSelfDetailContactInfo();

    // -- Whitepage searches --
    SearchResultEvent* searchForContacts(const string& nickname, const string& firstname,
					 const string& lastname);

    SearchResultEvent* searchForContacts(const string& nickname, const string& firstname,
					 const string& lastname, const string& email,
					 AgeRange age, Sex sex, unsigned char language, const string& city,
					 const string& state, unsigned short country,
					 const string& company_name, const string& department,
					 const string& position, bool only_online);

    SearchResultEvent* searchForContacts(unsigned int uin);

    SearchResultEvent* searchForContacts(const std::string& keyword);

    /*
     *  Poll must be called regularly (at least every 60 seconds)
     *  but I recommended 5 seconds, so timeouts work with good
     *  granularity.
     *  It is not related to the socket callback - the client using
     *  this library must select() on the sockets it gets signalled
     *  and call socket_cb when select returns a status flag on one
     *  of the sockets. ickle simply uses the gtk-- built in signal handlers
     *  to do all this.
     */

    // -- Network settings --
    void setLoginServerHost(const string& host);
    string getLoginServerHost() const;

    void setLoginServerPort(const unsigned short& port);
    unsigned short getLoginServerPort() const;

    void setBOSServerOverridePort(const bool& b);
    bool getBOSServerOverridePort() const;

    void setBOSServerPort(const unsigned short& port);
    unsigned short getBOSServerPort() const;

    void setPortRangeLowerBound(unsigned short lower);
    void setPortRangeUpperBound(unsigned short upper);
    unsigned short getPortRangeLowerBound() const;
    unsigned short getPortRangeUpperBound() const;

    void setUsePortRange(bool b);
    bool getUsePortRange() const;

    void Poll();

    void RegisterUIN();

    /* isConnected() is a convenience for the
     * client, it should correspond exactly to ConnectedEvents
     * & DisconnectedEvents the client gets
     */
    bool isConnected() const;
    
  };
}

#endif




