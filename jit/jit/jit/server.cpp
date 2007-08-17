/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Written by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 * C wrapper functions for C++ libicq2000 backend */

#include "icqtransport.h"
#include "wp_client.h"

#ifndef ZONE
#define ZONE "server.cpp"
#endif

/* session thread */
extern "C" void SessionCheck(session s) {
  WPclient * client = (WPclient *)s->client;
  client->Poll();
}

extern "C" void SendStatus(session s) {
  WPclient * client = (WPclient *)s->client;
  client->SetStatus();
}

/* from load_roster */
extern "C" void AddICQContact(contact c) {
  session s = c->s;
  WPclient * client = (WPclient *)s->client;
  ContactRef cc;

  cc = client->getContact(c->uin);
  if(!cc.get()){
    cc = new Contact(c->uin);
	client->addContact(cc);
  }
}

/* limit body to 160, nr must be correct */
extern "C" void SendSMS(session s,char * body,char * nr) {
  WPclient * client = (WPclient *)s->client;

  ContactRef nc(new Contact());
  nc->setMobileNo(string(nr));

  SMSMessageEvent*  message=new SMSMessageEvent(nc,string(body),true);
  client->SendEvent(message);
}

extern "C" void SendMessage(session s,char * body,unsigned char chat,UIN_t uin) {
  WPclient * client = (WPclient *)s->client;
  ContactRef c;

  string unutfed=string(body);

  c = client->getContact(uin);
  if(!c.get()){
    c = new Contact(uin);
  }

  NormalMessageEvent*  message=new NormalMessageEvent(c,unutfed);
  if(c->getStatus()==STATUS_DND||c->getStatus()==STATUS_OCCUPIED)
    message->setUrgent(true);
  client->SendEvent(message);
}


extern "C" void SendUrl(session s,char * url,char * desc,UIN_t uin) {
  WPclient * client = (WPclient *)s->client;
  ContactRef c;

  string description=string(desc?desc:"");
  string smart_url=string(url);
  
  c = client->getContact(uin);
  if(!c.get()){
    c = new Contact(uin);
  }
  
  URLMessageEvent* urlmessage=new URLMessageEvent(c,description,smart_url);
  if(c->getStatus()==STATUS_DND||c->getStatus()==STATUS_OCCUPIED)
    urlmessage->setUrgent(true);
  client->SendEvent(urlmessage);
}

extern "C" void SendAuthRequest(contact c,char * what) {
  session s = c->s;
  WPclient * client = (WPclient *)s->client;
  AuthReqEvent *ev;

  log_debug("Contact","Auth req for %d",c->uin);

  ContactRef nc = client->getContact(c->uin);
  if (!nc.get()) {
    nc = new Contact(c->uin);
  }
  
  ev = new AuthReqEvent(nc,string(what));
  client->SendEvent(ev);
}

extern "C" void SendAuthGiven(contact c) {
  session s = c->s;
  WPclient * client = (WPclient *)s->client;

  ContactRef nc = client->getContact(c->uin);
  if (!nc.get()) {
    nc = new Contact(c->uin);
  }

  AuthAckEvent* message = new AuthAckEvent(nc,true);
  client->SendEvent(message);
}

extern "C" void SendAuthDenied(contact c) {
  session s = c->s;
  WPclient * client = (WPclient *)s->client;

  ContactRef nc = client->getContact(c->uin);
  if (!nc.get()) {
    nc = new Contact(c->uin);
  }

  AuthAckEvent* message=new AuthAckEvent(nc,false);
  client->SendEvent(message);
}


extern "C" void SendRemoveContact(contact c) {
  session s = c->s;
  WPclient * client = (WPclient *)s->client;

  log_debug("Contact","Remove %d",c->uin);
  
  client->removeContact(c->uin); /* send to ICQ server */
}


extern "C" void SendSearchUINRequest(session s,UIN_t uin) {
  WPclient * client = (WPclient *)s->client;
  if (client->search_ev == NULL)
    client->search_ev = client->searchForContacts(uin);
  else {
    log_alert(ZONE,"Search in progress !!!!");
  }
}

extern "C" void SendSearchUsersRequest(session s, 
                       char *nick,
                       char *first,
                       char *last,
                       char *email,
                       char *city,
                       int age_min, 
                       int age_max,
                       int sex_int,
                       int online_only) {

  WPclient * client = (WPclient *)s->client;
  
  if (client->search_ev == NULL) {
    AgeRange age;
    Sex sex;
    bool online;
    const string n(nick),f(first),l(last),e(email),c(city),em("");

    if (online_only) 
      online = true;
    else
      online = false;

    if (age_min == 0) {
      if (age_max == 0) {
    age = range_NoRange;
      }
      else {
    if (age_max <= 22)
      age = range_18_22;
    else
      if (age_max <= 29)
        age = range_23_29;
      else
      if (age_max <= 39)
        age = range_30_39;
      else
        if (age_max <= 49)
          age = range_40_49;
        else
          if (age_max <= 59)
        age = range_50_59;
          else
        age = range_60_above;
      }

    }
    else {
      if (age_min > 59)
    age = range_60_above;           
      else
    if (age_min > 49)
      age = range_50_59;
    else
      if (age_min > 39)
        age = range_40_49;
      else
        if (age_min > 29)
          age = range_30_39;
        else
          if (age_min > 19)
        age = range_23_29;
          else
        age = range_18_22;
    }


    if (sex_int == 0)
      sex = SEX_UNSPECIFIED;
    else
      if (sex_int==1)
    sex = SEX_FEMALE;
      else
    if (sex_int==2)
      sex = SEX_MALE;

    client->search_ev = client->searchForContacts(n,f,l,e,
                          age,
                          sex,
                          0,//lang
                          c,
                          em,//state
                          0,//country
                          em,//company
                          em,//depa
                          em,//position
                          online);

  }
  else {
    log_alert(ZONE,"Search in progress !!!!");
  }
}

/* ----------------------------------------------------------------- */
extern "C" unsigned long GetLast(session s,UIN_t uin) {
  WPclient * client = (WPclient *)s->client;

  log_debug("GetLast","for %d",uin);

  ContactRef c = client->getContact(uin);
  if (c.get()&&(c->getStatus() == STATUS_OFFLINE)) {
    return c->get_last_online_time();
  }
  return 0;
}

/* ----------------------------------------------------------------- */
#define VCARD_ADD(iq, parent, name, val) \
    xmlnode_insert_cdata(xmlnode_insert_tag(xmlnode_insert_tag(iq,parent),name), \
    it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1)

extern "C" void ReSendVcard(void *arg);
extern "C" void SendVcard(session s, jpacket jp, ContactRef c);

/* heartbeat thread */
extern "C" result handleVcardWait(void * arg) {
  session s = (session) arg;

  if (s->exit_flag)
    return r_UNREG;  

  if (!s->vcard_get)
    return r_UNREG;

  mtq_send(s->q,NULL,ReSendVcard,arg);
  return r_UNREG;  
}

extern "C" void GetVcard(session s, jpacket jp, UIN_t uin) {
  WPclient * client = (WPclient *)s->client;
  
  log_debug(ZONE,"Get vcard for %d",uin);
  
  ContactRef c = client->getContact(uin);
  
  /* XXX check if we have user info already */


  if (s->vcard_get != NULL) {
    it_deliver(s->ti,jp->x);
    return;
  }
  
  /* if no user or not online and no data about him ?? */
  if (!c.get()) {
    c = new Contact(uin);
  }
  
  s->vcard_get = (vcard_wait)pmalloco(jp->p,sizeof(_vcard_wait));
  s->vcard_get->jp = jp;
  s->vcard_get->arg = (void *)(c.get());
  
  //send get info, detail info will keep contact for a while
  //in UserInfo event
  client->fetchDetailContactInfo(c);

  // update away message      
  if(c->getStatus()!=STATUS_ONLINE&&c->getStatus()!=STATUS_OFFLINE) {        
	MessageEvent *ev = new AwayMessageEvent(c);        
	client->SendEvent(ev);      
  }

  register_beat(3,handleVcardWait,(void *) s);
  //  return;
  //  SendVcard(s, jp, c);
}

/* session thread */
extern "C" void ReSendVcard(void *arg) {
  session s = (session) arg;

  if ((s->exit_flag)||(!s->vcard_get))
    return;  

  Contact *cc = (Contact *)s->vcard_get->arg;
  {
    ContactRef rc = cc;
    SendVcard(s,s->vcard_get->jp,rc);
  }

  s->vcard_get = NULL;
}

extern "C" void SendVcard(session s, jpacket jp, ContactRef c) {

  log_debug(ZONE,"Send vcard for %d",c->getUIN());

  char buf[50];
  string cur;
  register xmlnode q = jp->iq;
  register pool p = jp->p;
  
  /* XXX check if contact has info */
  cur = (c->getFirstName()+" "+c->getLastName());
    
  if (cur.size()!=1)
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"FN"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);
  
  
  register xmlnode t = xmlnode_insert_tag(q,"N");
  
  /* First Name */
  cur = c->getFirstName();
    
  if (cur.size())
    xmlnode_insert_cdata(xmlnode_insert_tag(t,"GIVEN"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

  /* Last Name */
  cur = c->getLastName();
    
  if (cur.size())
    xmlnode_insert_cdata(xmlnode_insert_tag(t,"FAMILY"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

  /* Alias */
  cur = c->getAlias();
    
  if (cur.size())
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"NICKNAME"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

  /* Email */
  cur = c->getEmail();
    
  if (cur.size())  {
    t = VCARD_ADD(q,"EMAIL","USERID",cur);
    xmlnode_insert_tag(t,"INTERNET");
    xmlnode_insert_tag(t,"PREF");
  }

  /* birth day */
  cur = c->getHomepageInfo().getBirthDate();
    
  if (cur.size() && cur != "Unspecified") {
	snprintf(buf,50,"%04d-%02d-%02d",
			 c->getHomepageInfo().birth_year,
			 c->getHomepageInfo().birth_month,
			 c->getHomepageInfo().birth_day);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"BDAY"),
						 buf, (unsigned int)-1);
  }

  /* Phone */
  cur = c->getMainHomeInfo().phone;
    
  if (cur.size()) {
    t = VCARD_ADD(q,"TEL","NUMBER",cur);
    xmlnode_insert_tag(t,"VOICE");
    xmlnode_insert_tag(t,"HOME");
  }

  /* FAX */
  cur = c->getMainHomeInfo().fax;
    
  if (cur.size())  {
    t = VCARD_ADD(q,"TEL","NUMBER",cur);
    xmlnode_insert_tag(t,"FAX");
    xmlnode_insert_tag(t,"HOME");
  }

  /* Mobile Phone */
  cur = c->getMobileNo();
    
  if (cur.size()) {
    t = VCARD_ADD(q,"TEL","NUMBER",cur);
    xmlnode_insert_tag(t,"VOICE");
    xmlnode_insert_tag(t,"CELL");   
  }


  /* Address */
  t = xmlnode_insert_tag(q,"ADR");
  xmlnode_insert_tag(t,"HOME");
  xmlnode_insert_tag(t,"EXTADD");
  
  cur = c->getMainHomeInfo().street;

  if (cur.size())
    xmlnode_insert_cdata(xmlnode_insert_tag(t,"STREET"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

  cur = c->getMainHomeInfo().city;

  if (cur.size())
    xmlnode_insert_cdata(xmlnode_insert_tag(t,"LOCALITY"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

  cur = c->getMainHomeInfo().state;
    
  if (cur.size())
    xmlnode_insert_cdata(xmlnode_insert_tag(t,"REGION"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

  cur = c->getMainHomeInfo().zip;
    
  if (cur.size())
    xmlnode_insert_cdata(xmlnode_insert_tag(t,"PCODE"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

  cur = c->getMainHomeInfo().getCountry();
    
  if (cur.size())
    xmlnode_insert_cdata(xmlnode_insert_tag(t,"COUNTRY"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

  /* HOMEPAGE */
  cur = c->getHomepageInfo().homepage;
    
  if (cur.size())
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"URL"),
                         it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

  /* WORK */
  cur = c->getWorkInfo().company_name;
  
  if (cur.size())  {
    t = VCARD_ADD(q,"ORG","ORGNAME",cur);
    
    cur = c->getWorkInfo().company_dept;
    
    if (cur.size())
      xmlnode_insert_cdata(xmlnode_insert_tag(t,"ORGUNIT"),
                           it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);
    
  }
  

  /* DESC */
  cur = c->getAboutInfo();

  {
    unsigned int uh = c->getLanIP();

    sprintf(buf,"IP: %d.%d.%d.%d:%d", (uh >> 24) & 0xff, (uh >> 16) & 0xff, 
            (uh >> 8) & 0xff, uh & 0xff,c->getLanPort());
    cur += ( string(buf) );
    
    if(c->getExtIP() != c->getLanIP()) {
      uh = c->getExtIP();
      sprintf(buf," / %d.%d.%d.%d:%d", (uh >> 24) & 0xff, (uh >> 16) & 0xff, 
              (uh >> 8) & 0xff, uh & 0xff,c->getExtPort());
      cur+=( string(buf) );
    }
  }
  cur+="\n";

  xmlnode_insert_cdata(xmlnode_insert_tag(q,"DESC"),
                       it_convert_windows2utf8(p,cur.c_str()), (unsigned int)-1);

 
  it_deliver(s->ti,jp->x);
}


/* ----------------------------------------------------------------- */
extern "C" void StartClient(session s) {
  int i;
  int j;
  WPclient * client;

  j = rand();
  
  if (j < 1) j = 1;
  if (j > 34534543) j = 1;
  
  i = j % s->ti->auth_hosts_count;
  
  client = new WPclient(s->uin,s->passwd);
  s->client = client;
  client->SetSession(s);
  
  client->setLoginServerHost(s->ti->auth_hosts[i]);
  client->setLoginServerPort(s->ti->auth_ports[i]);
  
  /* it will try to connect */
  /* find out client status */
  client->setStatus(STATUS_ONLINE,0);
  
  if (s->web_aware) 
    client->setWebAware(s->web_aware);
  
  /* make sure we have contacts before logging into bos */
  mtq_send(s->q, NULL, (void (*) (void *))it_contact_load_roster, (void *)s);
}

/* ----------------------------------------------------------------- */

extern "C" void EndClient(session s) {
  WPclient * client = (WPclient *)s->client;
  delete client;

  /* check it later in session_free */
  s->client = NULL;
}

extern "C" void FetchServerBasedContactList(session s) {
  WPclient * client = (WPclient *)s->client;
  client->fetchServerBasedContactList();
}

/* ----------------------------------------------------------------- */
/* IO IO IO IO IO IO IO IO IO IO IO IO IO IO IO IO IO IO IO IO IO IO */
/* ----------------------------------------------------------------- */

typedef struct recived_struct {
  session s;
  int len;
  unsigned char buf[1];
} _packet_recived, *packet_recived;

/* session thread */
extern "C" void PacketRecived(void * arg) {
  packet_recived packet = (packet_recived) arg;
  session s = packet->s;
  WPclient * client = (WPclient *)s->client;

  if (s->exit_flag) {
    log_alert(ZONE,"Packet to exiting session");
    free(packet);
    return;
  }

  client->RecvFromServer(packet->buf,packet->len);
  
  free(packet);
}

extern "C" void AuthSocketError(void * arg) {
  session s = (session) arg;
  WPclient *client = (WPclient *) s->client;
  if (s->exit_flag) 
    return;
  client->Disconnect(DisconnectedEvent::FAILED_LOWLEVEL);
}

extern "C" void BosSocketError(void * arg) {
  session s = (session) arg;
  WPclient *client = (WPclient *) s->client;
  if (s->exit_flag) 
    return;
  client->Disconnect(DisconnectedEvent::FAILED_LOWLEVEL);
}


extern "C" void it_server_auth(mio m, int state, void *arg, char *buffer, int len)
{
    session s = (session) arg;
    WPclient *client = NULL;
    packet_recived packet;    

    if (s == NULL) {
      mio_close(m);
      return;
    } 
    
    client = (WPclient *) s->client;

    if (s->exit_flag || client ==  NULL) {
      if (s->reference_count)
    s->reference_count--;

      mio_close(m);
      s->s_mio = NULL;
      return;
    }

    switch(state) {
    case MIO_NEW:
      log_debug(ZONE,"Session[%p,%s], Server Auth Connected",s,jid_full(s->id));
      s->s_mio = m;
      if (s->reference_count)
    s->reference_count--;
      return;      
    case MIO_CLOSED:
      if (s->reference_count)
    s->reference_count--;

      log_debug(ZONE,"Session[%p,%s], Server Auth socket closed",s,jid_full(s->id));
      s->s_mio = NULL;

      if (client->isCookieData()==0) {
        mtq_send(s->q,NULL,AuthSocketError,(void*)s);
      }
      return;
    case MIO_ERROR:
      log_alert(ZONE,"Session[%s]. Auth. Socket error !",jid_full(s->id));
      return;
    case MIO_BUFFER:
      packet = (packet_recived) malloc(sizeof(_packet_recived)+len);
      packet->len = len;
      packet->s = s;
      memcpy(packet->buf,buffer,len);

      mtq_send(s->q,NULL,PacketRecived,(void*)packet);
      break;

    default:
      return;
    }
}

extern "C" void it_server_bos(mio m, int state, void *arg, char *buffer, int len) {
  session s = (session) arg;
  packet_recived packet;
  
  if (s == NULL) {
    mio_close(m);
    return;
  } 
  
  if (s->exit_flag) {
    if (s->reference_count)
      s->reference_count--;
    
    mio_close(m);
    s->s_mio = NULL;
    return;
  }
  
  switch(state)
    {
    case MIO_NEW:
      s->s_mio = m;

      if (s->reference_count)
    s->reference_count--;

      return;
      
    case MIO_CLOSED:
      log_debug(ZONE,"Session[%s], Server Bos socket closed",jid_full(s->id));
      s->s_mio = NULL; /* it will be freed by MIO */

      if (s->reference_count)
        s->reference_count--;

      mtq_send(s->q,NULL,BosSocketError,(void*)s);      
      return;
    case MIO_ERROR:
      log_alert(ZONE,"Session[%s]. Bos. Socket error !",jid_full(s->id));
      return;
    case MIO_BUFFER:
      packet = (packet_recived) malloc(sizeof(_packet_recived)+len);
      packet->len = len;
      packet->s = s;
      memcpy(packet->buf,buffer,len);

      mtq_send(s->q,NULL,PacketRecived,(void*)packet);
      break;
    default:
      return;
    }
}
