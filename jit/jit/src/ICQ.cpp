/*
 * ICQ Subtypes
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

#include <libicq2000/ICQ.h>

#include <libicq2000/Capabilities.h>

#include "sstream_fix.h"
#include <memory>

using std::string;
using std::list;
using std::ostringstream;
using std::auto_ptr;

namespace ICQ2000 {

  // ----------------- ICQSubtypes ----------------

  ICQSubType::ICQSubType()
    : m_flags(0x0000) { }

  ICQSubType* ICQSubType::ParseICQSubType(Buffer& b, bool adv, bool ack) {
    unsigned char type, flags;
    b >> type
      >> flags;
    
    bool multi = (flags & MSG_Flag_Multi);

    ICQSubType *ist;
    switch(type) {
    case MSG_Type_Normal:
      ist = new NormalICQSubType(multi);
      break;
    case MSG_Type_URL:
      ist = new URLICQSubType();
      break;
    case MSG_Type_SMS:
      ist = new SMSICQSubType();
      break;
    case MSG_Type_AuthReq:
      ist = new AuthReqICQSubType();
      break;
    case MSG_Type_AuthRej:
      ist = new AuthRejICQSubType();
      break;
    case MSG_Type_AuthAcc:
      ist = new AuthAccICQSubType();
      break;
    case MSG_Type_EmailEx:
      ist = new EmailExICQSubType();
      break;
    case MSG_Type_WebPager:
      ist = new WebPagerICQSubType();
      break;
    case MSG_Type_UserAdd:
      ist = new UserAddICQSubType();
      break;
    case MSG_Type_AutoReq_Away:
    case MSG_Type_AutoReq_Occ:
    case MSG_Type_AutoReq_NA:
    case MSG_Type_AutoReq_DND:
    case MSG_Type_AutoReq_FFC:
      ist = new AwayMsgSubType(type);
      break;
    default:
      {
	ostringstream ostr;
	ostr << "Unknown ICQ Subtype: 0x" << std::hex << (int) type;
	throw ParseException(ostr.str());
      }
    }

    if (dynamic_cast<UINICQSubType*>(ist) != NULL) {
      UINICQSubType *ust = dynamic_cast<UINICQSubType*>(ist);
      ust->setAdvanced(adv);
      ust->setACK(ack);
      /* There is nothing in the encoding of the ICQ subtype that
	 distinguishes whether it is an ack or not - this is implied
	 by the protocol layer above this. As such it has to be passed
	 as an argument and set body the rest of parsing continues */
    }
    ist->setFlags(flags);
    ist->ParseBody(b);

    return ist;
  }

  void ICQSubType::Output(Buffer& b) const {
    b << getType()
      << getFlags();

    OutputBody(b);
  }

  UINICQSubType::UINICQSubType()
    : m_source(0), m_destination(0), m_advanced(false),
      m_ack(false), m_urgent(false),
      m_tocontactlist(false), m_status(0) { }

  UINICQSubType::UINICQSubType(unsigned int s, unsigned int d)
    : m_source(s), m_destination(d), m_advanced(false),
      m_ack(false), m_urgent(false),
      m_tocontactlist(false), m_status(0) { }

  unsigned int UINICQSubType::getSource() const { return m_source; }

  unsigned int UINICQSubType::getDestination() const { return m_destination; }

  void UINICQSubType::setDestination(unsigned int d) { m_destination = d; }

  void UINICQSubType::setSource(unsigned int s) { m_source = s; }

  bool UINICQSubType::isAdvanced() const { return m_advanced; }

  void UINICQSubType::setAdvanced(bool b) { m_advanced = b; }

  bool UINICQSubType::isACK() const { return m_ack; }

  void UINICQSubType::setACK(bool b) { m_ack = b; }

  unsigned short UINICQSubType::getStatus() const { return m_status; }

  void UINICQSubType::setStatus(unsigned short s) { m_status = s; }

  bool UINICQSubType::isUrgent() const
  {
    return m_urgent;
  }

  void UINICQSubType::setUrgent(bool b)
  {
    m_urgent = b;
  }

  bool UINICQSubType::isToContactList() const
  {
    return m_tocontactlist;
  }

  void UINICQSubType::setToContactList(bool b)
  {
    m_tocontactlist = b;
  }

  void UINICQSubType::setAwayMessage(const string& m)
  {
    m_away_message = m;
  }

  string UINICQSubType::getAwayMessage() const
  {
    return m_away_message;
  }

  void UINICQSubType::ParseBody(Buffer& b)
  {
    if (m_advanced) {
      unsigned short priority;
      b >> m_status
	>> priority;
      m_urgent = priority & Priority_Urgent;
      m_tocontactlist = priority & Priority_ToContactList;
      if (!m_urgent && priority != Priority_Normal && priority != 0)
	m_tocontactlist = true;
      // the official clients seem fairly lenient on what they'll accept in status flags
    }

    if (!m_ack) {
      ParseBodyUIN(b);
    } else {
      ParseBodyUINACK(b);
    }
    
  }

  void UINICQSubType::OutputBody(Buffer& b) const
  {
    if (m_advanced) {
      b << (unsigned short)m_status;
      unsigned short priority = 0x0000;

      if (!m_ack) {
	priority = Priority_Normal;
	if (m_urgent) priority = Priority_Urgent;
	if (m_tocontactlist) priority = Priority_ToContactList;
      }

      b << priority;
    }

    if (!m_ack) {
      OutputBodyUIN(b);
    } else {
      OutputBodyUINACK(b);
    }
    
  }

  void UINICQSubType::OutputBodyUINACK(Buffer& b) const
  {
    b.PackUint16TranslatedNull(m_away_message);
  }

  void UINICQSubType::ParseBodyUINACK(Buffer& b)
  {
    /* whatever the message type, ACKs are always the same
       - the current away message */
    b.UnpackUint16TranslatedNull(m_away_message);
  }

  NormalICQSubType::NormalICQSubType(bool multi)
    : m_multi(multi), m_foreground(0x00000000),
      m_background(0x00ffffff), m_encoding(ENCODING_UNSPECIFIED) { }

  NormalICQSubType::NormalICQSubType(const string& msg)
    : m_message(msg), m_foreground(0x00000000),
      m_background(0x00ffffff), m_encoding(ENCODING_UNSPECIFIED) { }

  string NormalICQSubType::getMessage() const { return m_message; }
  
  bool NormalICQSubType::isMultiParty() const { return m_multi; }

  void NormalICQSubType::setMessage(const string& msg) { m_message = msg; }
  
  Encoding NormalICQSubType::getTextEncoding() const { return m_encoding; }
  
  void NormalICQSubType::setTextEncoding( Encoding e ) { m_encoding = e; }

  void NormalICQSubType::ParseBodyUIN(Buffer& b) {
    b.UnpackUint16StringNull(m_message);
    b.ServerToClient(m_message);

    if (m_advanced) {
      b >> m_foreground
	>> m_background;
    if ( m_message.length() )
	  {
		  unsigned int capslen;
		  b >> capslen;
		  Capabilities caps;
		  caps.ParseString( b, capslen );
		  if ( caps.has_capability_flag( Capabilities::ICQUTF8 ) )
			m_encoding = ENCODING_UTF8;
	  }
    } else {
      m_foreground = 0x00000000;
      m_background = 0x00ffffff;
    }
  }

  void NormalICQSubType::OutputBodyUIN(Buffer& b) const {
    b.PackUint16TranslatedNull(m_message);

    if (m_advanced) {
      b << (unsigned int)m_foreground
	<< (unsigned int)m_background;
      if (m_encoding==ENCODING_UTF8)
	  {
		b << (unsigned int)38;  // size of one capability
		Capabilities caps;
		caps.set_capability_flag( Capabilities::ICQUTF8 );
		caps.OutputString( b );
	  } 
    }
  }

  void NormalICQSubType::ParseBodyUINACK(Buffer& b)
  {
    b.UnpackUint16TranslatedNull(m_away_message);
    b.advance(8);
  }

  void NormalICQSubType::OutputBodyUINACK(Buffer& b) const
  {
    /* hmmff.. Normal messages differ from all the other ACKs by
       having these two pointless ints. */
    b.PackUint16TranslatedNull(m_away_message);
    if (m_advanced) {
      b << (unsigned int)0x00000000
	<< (unsigned int)0xffffffff;
    }
  }

  unsigned short NormalICQSubType::Length() const {
    string text = m_message;
    Translator::LFtoCRLF(text);

    return text.size() + (m_advanced ? 13 : 5);
  }

  unsigned char NormalICQSubType::getType() const { return MSG_Type_Normal; }

  void NormalICQSubType::setForeground(unsigned int f) { m_foreground = f; }

  void NormalICQSubType::setBackground(unsigned int b) { m_background = b; }
  
  unsigned int NormalICQSubType::getForeground() const { return m_foreground; }

  unsigned int NormalICQSubType::getBackground() const { return m_background; }

  URLICQSubType::URLICQSubType()
    { }

  URLICQSubType::URLICQSubType(const string& msg, const string& url)
    : m_message(msg), m_url(url) { }

  string URLICQSubType::getMessage() const { return m_message; }

  string URLICQSubType::getURL() const { return m_url; }

  void URLICQSubType::setMessage(const string& msg) { m_message = msg; }

  void URLICQSubType::setURL(const string& url) { m_url = url; }

  void URLICQSubType::ParseBodyUIN(Buffer& b) {
    string text;
    b.UnpackUint16StringNull(text);
    
    /*
     * Format is [message] 0xfe [url]
     */
    int l = text.find( 0xfe );
    if (l != -1) {
      m_message = text.substr( 0, l );
      m_url = text.substr( l+1 );
    } else {
      m_message = text;
      m_url = "";
    }
    b.ServerToClient(m_message);
    b.ServerToClient(m_url);

  }

  void URLICQSubType::OutputBodyUIN(Buffer& b) const {
    if (m_ack) {
      b.PackUint16StringNull("");
    } else {
      ostringstream ostr;

      string message = m_message;
      string url = m_url;
      b.ClientToServer(message);
      b.ClientToServer(url);     // I think url should be translated too ??
      ostr << message << (unsigned char)0xfe << url;

      b.PackUint16StringNull( ostr.str() );
    }
  }

  unsigned short URLICQSubType::Length() const {
    string text = m_message + m_url;
    Translator::LFtoCRLF(text);

    return text.size() + 6;
  }

  unsigned char URLICQSubType::getType() const { return MSG_Type_URL; }

  AwayMsgSubType::AwayMsgSubType(unsigned char type)
   : m_type(type) { }

  AwayMsgSubType::AwayMsgSubType(Status s)
  {
    
    switch(s) {
    case STATUS_AWAY:
      m_type = MSG_Type_AutoReq_Away;
      break;
    case STATUS_OCCUPIED:
      m_type = MSG_Type_AutoReq_Occ;
      break;
    case STATUS_NA:
      m_type = MSG_Type_AutoReq_NA;
      break;
    case STATUS_DND:
      m_type = MSG_Type_AutoReq_DND;
      break;
    case STATUS_FREEFORCHAT:
      m_type = MSG_Type_AutoReq_FFC;
      break;
    default:
      m_type = MSG_Type_AutoReq_Away;
    }

  }

  void AwayMsgSubType::ParseBodyUIN(Buffer& b) {
    // dummy
    string dummy;
    b.UnpackUint16StringNull(dummy);
  }

  void AwayMsgSubType::OutputBodyUIN(Buffer& b) const {
    // dummy
    string dummy;
    b.PackUint16StringNull( dummy );
  }

  unsigned short AwayMsgSubType::Length() const {
    // This doesn't appear to be called when sending an away message
    // response, so I'm leaving this as 9.
    return 9;
  }

  unsigned char AwayMsgSubType::getType() const { return m_type; }

  unsigned char AwayMsgSubType::getFlags() const { return 0x03; }

  SMSICQSubType::SMSICQSubType() { }

  string SMSICQSubType::getMessage() const { return m_message; }

  SMSICQSubType::Type SMSICQSubType::getSMSType() const { return m_type; }

  void SMSICQSubType::ParseBody(Buffer& b) {
    /*
     * Here we go... this is a biggy
     */

    /* Next 21 bytes
     * Unknown 
     * 01 00 00 20 00 0e 28 f6 00 11 e7 d3 11 bc f3 00 04 ac 96 9d c2
     */
    b.advance(21);

    /* Delivery status
     *  0x0000 = SMS
     *  0x0002 = SMS Receipt Success
     *  0x0003 = SMS Receipt Failure
     */
    unsigned short del_stat;
    b >> del_stat;
    switch (del_stat) {
    case 0x0000:
      m_type = SMS;
      break;
    case 0x0002:
    case 0x0003:
      m_type = SMS_Receipt;
      break;
    default:
      throw ParseException("Unknown Type for SMS ICQ Subtype");
    }

    /*
     * A Tag for the type, can be:
     * - "ICQSMS" NULL (?)
     * - "IrCQ-Net Invitation"
     * - ...
     * 07 00 00 00 49 43 51 53 4d 53 00
     * ---length-- ---string-----------
     */
    string tagstr;
    b.UnpackUint32String(tagstr);

    if (tagstr != string("ICQSMS")+'\0') {
      ostringstream ostr;
      ostr << "Unknown SNAC 0x0004 0x0007 ICQ SubType 0x001a tag string: " << tagstr;
      throw ParseException(ostr.str());
    }

    /* Next 3 bytes
     * Unknown
     * 00 00 00
     */
    b.advance(3);


    /* Length till end
     * 4 bytes
     */
    unsigned int msglen;
    b >> msglen;

    string xmlstr;
    b.UnpackUint32String(xmlstr);

    string::iterator s = xmlstr.begin();
    auto_ptr<XmlNode> top(XmlNode::parse(s, xmlstr.end()));

    if (top.get() == NULL) throw ParseException("Couldn't parse xml data in Message SNAC");

    if (m_type == SMS) {

      // -------- Normal SMS Message ---------
      if (top->getTag() != "sms_message") throw ParseException("No <sms_message> tag found in xml data");
      XmlBranch *sms_message = dynamic_cast<XmlBranch*>(top.get());
      if (sms_message == NULL || !sms_message->exists("text")) throw ParseException("No <text> tag found in xml data");
      XmlLeaf *text = sms_message->getLeaf("text");
      if (text == NULL) throw ParseException("<text> tag is not a leaf in xml data");
      m_message = text->getValue();
      
      /**
       * Extra fields
       * senders_network is always blank from my mobile
       */
      XmlLeaf *source = sms_message->getLeaf("source");
      if (source != NULL) m_source = source->getValue();

      XmlLeaf *sender = sms_message->getLeaf("sender");
      if (sender != NULL) m_sender = sender->getValue();

      XmlLeaf *senders_network = sms_message->getLeaf("senders_network");
      if (senders_network != NULL) m_senders_network = senders_network->getValue();

      XmlLeaf *time = sms_message->getLeaf("time");
      if (time != NULL) m_time = time->getValue();

      // ----------------------------------

    } else if (m_type == SMS_Receipt) {

      // -- SMS Delivery Receipt Success --
      if (top->getTag() != "sms_delivery_receipt") throw ParseException("No <sms_delivery_receipt> tag found in xml data");
      XmlBranch *sms_rcpt = dynamic_cast<XmlBranch*>(top.get());
      if (sms_rcpt == NULL) throw ParseException("No tags found in <sms_delivery_receipt>");

      XmlLeaf *message_id = sms_rcpt->getLeaf("message_id");
      if (message_id != NULL) m_message_id = message_id->getValue();

      XmlLeaf *destination = sms_rcpt->getLeaf("destination");
      if (destination != NULL) m_destination = destination->getValue();

      XmlLeaf *delivered = sms_rcpt->getLeaf("delivered");
      m_delivered = false;
      if (delivered != NULL && delivered->getValue() == "Yes") m_delivered = true;

      XmlLeaf *text = sms_rcpt->getLeaf("text");
      if (text != NULL) m_message = text->getValue();

      XmlLeaf *submission_time = sms_rcpt->getLeaf("submition_time"); // can they not spell!
      if (submission_time != NULL) m_submission_time = submission_time->getValue();

      XmlLeaf *delivery_time = sms_rcpt->getLeaf("delivery_time");
      if (delivery_time != NULL) m_delivery_time = delivery_time->getValue();

      // could do with parsing errors for Failure

      // ---------------------------------

    }
      
  }

  void SMSICQSubType::OutputBody(Buffer& b) const {  }

  unsigned short SMSICQSubType::Length() const { return 0; }

  unsigned char SMSICQSubType::getType() const { return MSG_Type_SMS; }

  AuthReqICQSubType::AuthReqICQSubType()
    { }

  AuthReqICQSubType::AuthReqICQSubType(const string& alias, const string& firstname,
				       const string& lastname, const string& email, bool auth,
				       const string& msg)
    : m_alias(alias), m_firstname(firstname), m_lastname(lastname),
      m_email(email), m_message(msg), m_auth(auth)  { }

  string AuthReqICQSubType::getMessage() const { return m_message; }
  
  void AuthReqICQSubType::ParseBodyUIN(Buffer& b) {
    string text;

    b.UnpackUint16StringNull(text);

    /*
     *
     * The packet does have nickname, first and last name fields,
     * but they're better to fetch with user details lookup request
     * for different versions of Windows client either have or don't
     * have them in packets they send. <konst>
     *
     */

    list<string> fields;
    string_split( text, string("\xfe"), 6, fields);

    list<string>::iterator iter = fields.begin();
    m_alias = b.ServerToClientCC(*(iter++));
    m_firstname = b.ServerToClientCC(*(iter++));
    m_lastname = b.ServerToClientCC(*(iter++));
    m_email = b.ServerToClientCC(*(iter++));
    m_auth = ((*(iter++)) == "1");
    m_message = b.ServerToClientCC(*(iter++));
  }

  void AuthReqICQSubType::OutputBodyUIN(Buffer& b) const {
    ostringstream ostr;
    ostr << b.ClientToServerCC(m_alias) << (unsigned char)0xfe
	 << b.ClientToServerCC(m_firstname) << (unsigned char)0xfe
	 << b.ClientToServerCC(m_lastname) << (unsigned char)0xfe
	 << b.ClientToServerCC(m_email) << (unsigned char)0xfe
	 << (m_auth ? "1" : "0") << (unsigned char)0xfe
	 << b.ClientToServerCC(m_message);
    
    b.PackUint16StringNull( ostr.str() );
  }

  unsigned short AuthReqICQSubType::Length() const {
    return 0;
  }

  unsigned char AuthReqICQSubType::getType() const { return MSG_Type_AuthReq; }
  
  AuthRejICQSubType::AuthRejICQSubType()
  { }

  AuthRejICQSubType::AuthRejICQSubType(const string& msg)
    : m_message(msg)  { }

  string AuthRejICQSubType::getMessage() const { return m_message; }
  
  void AuthRejICQSubType::setMessage(const string& msg) { m_message = msg; }

  void AuthRejICQSubType::ParseBodyUIN(Buffer& b) {
    b.UnpackUint16StringNull(m_message);
    b.ServerToClient(m_message);
  }

  void AuthRejICQSubType::OutputBodyUIN(Buffer& b) const {
    b.PackUint16TranslatedNull(m_message);
  }

  unsigned short AuthRejICQSubType::Length() const {
    return m_message.size()+3;
  }

  unsigned char AuthRejICQSubType::getType() const { return MSG_Type_AuthRej; }

  AuthAccICQSubType::AuthAccICQSubType()
  { }

  void AuthAccICQSubType::ParseBodyUIN(Buffer& b) {
    string nothing_much;
    b.UnpackUint16StringNull(nothing_much);
  }

  void AuthAccICQSubType::OutputBodyUIN(Buffer& b) const {
    b.PackUint16StringNull("");
  }

  unsigned short AuthAccICQSubType::Length() const {
    return 0;
  }

  unsigned char AuthAccICQSubType::getType() const { return MSG_Type_AuthAcc; }

  // ============================================================================
  //  Email Express message
  // ============================================================================

  EmailExICQSubType::EmailExICQSubType() { }

  void EmailExICQSubType::ParseBody(Buffer& b) {
    string text;

    b.UnpackUint16StringNull(text);

    list<string> fields;
    string_split( text, string("\xfe"), 6, fields);

    list<string>::iterator iter = fields.begin();
    m_sender = b.ServerToClientCC(*(iter++));
    ++iter; // ???
    ++iter; // ???
    m_email = b.ServerToClientCC(*(iter++));
    ++iter; // ???
    m_message = b.ServerToClientCC(*(iter++));
  }

  void EmailExICQSubType::OutputBody(Buffer& b) const {
    // never sent
  }

  unsigned short EmailExICQSubType::Length() const {
    return 0;
  }

  unsigned char EmailExICQSubType::getType() const { return MSG_Type_EmailEx; }

  string EmailExICQSubType::getMessage() const { return m_message; }

  string EmailExICQSubType::getEmail() const { return m_email; }

  string EmailExICQSubType::getSender() const { return m_sender; }

  // ============================================================================
  //  Web Pager message
  // ============================================================================

  WebPagerICQSubType::WebPagerICQSubType() { }

  void WebPagerICQSubType::ParseBody(Buffer& b) {
    string text;

    b.UnpackUint16StringNull(text);

    list<string> fields;
    string_split( text, string("\xfe"), 6, fields);

    list<string>::iterator iter = fields.begin();
    m_sender = b.ServerToClientCC(*(iter++));
    ++iter; // ???
    ++iter; // ???
    m_email = b.ServerToClientCC(*(iter++));
    ++iter; // ???
    m_message = b.ServerToClientCC(*(iter++));
  }

  void WebPagerICQSubType::OutputBody(Buffer& b) const {
    // never sent
  }

  unsigned short WebPagerICQSubType::Length() const {
    return 0;
  }

  unsigned char WebPagerICQSubType::getType() const { return MSG_Type_WebPager; }

  string WebPagerICQSubType::getMessage() const { return m_message; }

  string WebPagerICQSubType::getEmail() const { return m_email; }

  string WebPagerICQSubType::getSender() const { return m_sender; }

  // ============================================================================
  //  "You were added" message
  // ============================================================================

  UserAddICQSubType::UserAddICQSubType() { }

  UserAddICQSubType::UserAddICQSubType(const string& alias, const string& firstname,
				       const string& lastname, const string& email, bool auth)
    : m_alias(alias), m_firstname(firstname), m_lastname(lastname),
      m_email(email), m_auth(auth) { }

  void UserAddICQSubType::ParseBodyUIN(Buffer& b)
  {
    string text;

    b.UnpackUint16StringNull(text);

    /*
     *
     * The packet does have nickname, first and last name fields,
     * but they're better to fetch with user details lookup request
     * for different versions of Windows client either have or don't
     * have them in packets they send. <konst>
     *
     */

    list<string> fields;
    string_split( text, string("\xfe"), 5, fields);

    list<string>::iterator iter = fields.begin();
    m_alias = b.ServerToClientCC(*(iter++));
    m_firstname = b.ServerToClientCC(*(iter++));
    m_lastname = b.ServerToClientCC(*(iter++));
    m_email = b.ServerToClientCC(*(iter++));
    m_auth = ((*(iter++)) == "1");
  }

  void UserAddICQSubType::OutputBodyUIN(Buffer& b) const {
    ostringstream ostr;
    ostr << b.ClientToServerCC(m_alias) << (unsigned char)0xfe
	 << b.ClientToServerCC(m_firstname) << (unsigned char)0xfe
	 << b.ClientToServerCC(m_lastname) << (unsigned char)0xfe
	 << b.ClientToServerCC(m_email) << (unsigned char)0xfe
	 << (m_auth ? "1" : "0") << (unsigned char)0xfe;
    
    b.PackUint16StringNull( ostr.str() );
  }

  unsigned short UserAddICQSubType::Length() const { return 0; }

  unsigned char UserAddICQSubType::getType() const { return MSG_Type_UserAdd; }

  void string_split(const string& in, const string& sep, int count, std::list<string>& fields)
  {
    // oh for perl.. :-)
    string::size_type lpos = 0, pos = 0;
    while (lpos != in.size()) {
      pos = in.find(sep, lpos);
      fields.push_back(in.substr(lpos, pos-lpos));
      if (pos == string::npos) {
	lpos = in.size();
      } else {
	lpos = pos + sep.size();
      }
    }

    // add blank fields if necessary
    count -= fields.size();
    while(count > 0) {
      fields.push_back(string());
      --count;
    }
    
  }
  
}
