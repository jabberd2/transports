/**
 * MessageHandler
 *
 * Copyright (C) 2002 Barnaby Gray <barnaby@beedesign.co.uk>
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

#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <time.h>

#include <libicq2000/Contact.h>
#include <libicq2000/events.h>

namespace ICQ2000 {
  
  class ContactList;
  class ICQSubType;
  class UINICQSubType;
  class MessageEvent;
  class ICQMessageEvent;

  /**
   *  This is the central place all message signalling to the client
   *  goes through. They can come from three sources: 1. A MessageSNAC
   *  through the server, 2. A SrvResponseSNAC - offline message, 3. a
   *  direct message
   */

  class Client;
  void messaged_cb(Client * client ,MessageEvent* ev); 
  void messageack_cb(Client * client ,MessageEvent* ev);
  void want_auto_resp_cb(Client * client ,ICQMessageEvent* ev);
  void logger_cb(Client * client ,LogEvent* ev);

  class MessageHandler  {
   private:
    ContactRef m_self_contact;
    ContactList *m_contact_list;
    
    MessageEvent* ICQSubTypeToEvent(ICQSubType *st, ContactRef& contact, bool& adv);
    ICQMessageEvent* UINICQSubTypeToEvent(UINICQSubType *st, const ContactRef& contact);

    ContactRef lookupUIN(unsigned int uin);
    ContactRef lookupEmail(const std::string& email, const std::string& alias);
    ContactRef lookupMobile(const std::string& m);

    UINICQSubType* EventToUINICQSubType(MessageEvent *ev);

    void SignalLog(LogEvent::LogType type, const std::string& msg);
    
  public:
    MessageHandler(ContactRef self, ContactList *cl);

    Client *client;
    void setClient(Client * arg) {
      client = arg;
    }

    // incoming messages
    bool handleIncoming(ICQSubType* icq, time_t t = 0);

    // outgoing messages
    UINICQSubType* handleOutgoing(MessageEvent *ev);

    // incoming ACKs
    void handleIncomingACK(MessageEvent *ev, UINICQSubType* icq);

    //    SigC::Signal1<void,MessageEvent*> messaged;
    //    SigC::Signal1<void,MessageEvent*> messageack;
    //    SigC::Signal1<void,ICQMessageEvent*> want_auto_resp;
    //    SigC::Signal1<void,LogEvent*> logger;
  };
}

#endif
