/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Written by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

#ifndef wp_Client_h
#define wp_Client_h

#include <libicq2000/TLV.h>
#include <libicq2000/UserInfoBlock.h>

#include <libicq2000/Client.h>
#include <libicq2000/SNAC.h>

#include "src/sstream_fix.h"

using namespace ICQ2000;

class WPclient : public Client {

 private:
  session sesja;

 public:
  WPclient(unsigned int uin,const string& passwd);
    
  virtual ~WPclient();

  SearchResultEvent* search_ev;

  void SetSession(session arg);

  void WPInit();

  /* to send func */
  void SetStatus();


  /* our func */
  void sendContactPresence(const unsigned int uin, const string& s="");

  /* override Client func */
  void SignalConnected(ConnectedEvent *ev);
  void SignalDisconnected(DisconnectedEvent *ev);
  void SignalMessaged(MessageEvent *ev);
  void SignalMessageAck(MessageEvent *ev);    
  void SignalContactList(ContactListEvent *ev);
  void SignalServerContactEvent(ServerBasedContactEvent* ev);
  void SignalUserInfoChangeEvent(UserInfoChangeEvent *ev);
  void SignalStatusChangeEvent(StatusChangeEvent *ev);
  void SignalAwayMessageEvent(ICQMessageEvent *ev);
  void SignalSearchResultEvent(SearchResultEvent *ev);
  void SignalLogE(LogEvent *ev);

  void Send(Buffer& b);

  void SocketDisconnect();
  void SocketConnect(const char* host,int port,int type);

  Status show2status(const char* _show) {
    if(_show==NULL)return STATUS_ONLINE;
    string show=_show;
    if (show=="away")
      return STATUS_AWAY;
    if (show=="busy")
      return STATUS_OCCUPIED;
    if (show=="chat")
      return STATUS_FREEFORCHAT;
    if (show=="dnd")
      return STATUS_DND;
    if (show=="xa")
      return STATUS_NA;
    
    return STATUS_ONLINE;
  }

};


#endif
