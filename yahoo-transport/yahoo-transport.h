/*

    Yahoo! Transport 2 for the Jabber Server
    Copyright (c) 2002, Paul Curtis
    Portions of this program are derived from GAIM (yahoo.c)

    This file is part of the Jabber Yahoo! Transport. (yahoo-transport-2)

    The Yahoo! Transport is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    The Yahoo! Transport is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Yahoo! Transport; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    $Id: yahoo-transport.h,v 1.27 2004/07/01 16:06:34 pcurtis Exp $
*/


#include "jabberd.h"
#include <sys/utsname.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <iconv.h>
#include <glib.h>
#include "md5.h"

#ifdef _JCOMP
#define YAHOO_VERSION	"2.3.2-JCR"
#else
#define YAHOO_VERSION	"2.3.2"
#endif
#define YAHOO_SUB_VERSION	"stable"
#define YAHOO_GAIM_VERSION	"1.150"
#define NS_PRINT(x) xmlnode_get_attrib(x,"xmlns")
#define NS_STATS "jabber:iq:stats"
#define NS_DISCO_INFO "http://jabber.org/protocol/disco#info"
#define NS_DISCO_ITEMS "http://jabber.org/protocol/disco#items"

/*
   Statistics for the transport. See JEP-0039 for details
*/
typedef struct yahoo_transport_stats_struct {
  unsigned long packets_in;
  unsigned long packets_out;
  unsigned long bytes_in;
  unsigned long bytes_out;
  time_t start;
} _yahoo_transport_stats, *yahoo_transport_stats;



/*
  Yahoo! transport instance structure. This structure contains all the information needed
  for the transport to operate. There is one of these structures per instance of the transport.
*/

typedef struct yahoo_instance_struct {
  struct yahoo_transport_stats_struct *stats;		// JET-XXXX stats
  instance i;		// The instance of the transport provided by the server at startup
#ifndef _JCOMP
  xdbcache xc;		// This cache is provided by the instance for access to the configuration
  mtq q;		// packet delivery queue (needed by the server)
#else
  void *xc;		// This cache is provided by the instance for access to the configuration
  GThreadPool *q;
#endif
  xmlnode cfg;		// contains the server configuration for this instance
#ifndef _JCOMP
  pth_mutex_t lock;	// Used for session locking
#else
  GMutex *lock;
#endif
  xht user;		// This contains a hash of user session
  char *server;		// Yahoo server to connect to
  int port;		// Yahoo server port number to connect to
  char *charmap;	// character set map. Used with 'iconv' to map to/from UTF-8
  int timer_interval;	// milliseconds between auth checks
  int timeout;		// How long to wait for auth
  int mail;		// Send new mail notifications
} _yahoo_instance, *yahoo_instance;


#define	YAHOO_CONNECTED		0
#define	YAHOO_CONNECTING	1
#define	YAHOO_NEW_SESSION	2
#define	YAHOO_NOT_REGISTERED	3
#define	YAHOO_UNREGISTER	4
#define	YAHOO_CANCEL		5

/*
  Yahoo! User structure. There is one of these structures per current users of the transport.
  Each user's structure is stored in a hash that is maintained by the instance.
*/

struct yahoo_data {
	mio fd;
	jid me;
	int connection_state;
	guchar *rxqueue;
	int rxlen;
	int current_status;
	int registration_required;
	gboolean logged_in;
	char *username;
	char *password;
        char *session_key;
	char displayname[64];
	yahoo_instance yi;
	xht contacts;
//	mtq queue;
//	pool p;
	char *buf;
	int len;
        int last_mail_count;
};



enum yahoo_service { /* these are easier to see in hex */
	YAHOO_SERVICE_LOGON = 1,
	YAHOO_SERVICE_LOGOFF,
	YAHOO_SERVICE_ISAWAY,
	YAHOO_SERVICE_ISBACK,
	YAHOO_SERVICE_IDLE, /* 5 (placemarker) */
	YAHOO_SERVICE_MESSAGE,
	YAHOO_SERVICE_IDACT,
	YAHOO_SERVICE_IDDEACT,
	YAHOO_SERVICE_MAILSTAT,
	YAHOO_SERVICE_USERSTAT, /* 0xa */
	YAHOO_SERVICE_NEWMAIL,
	YAHOO_SERVICE_CHATINVITE,
	YAHOO_SERVICE_CALENDAR,
	YAHOO_SERVICE_NEWPERSONALMAIL,
	YAHOO_SERVICE_NEWCONTACT,
	YAHOO_SERVICE_ADDIDENT, /* 0x10 */
	YAHOO_SERVICE_ADDIGNORE,
	YAHOO_SERVICE_PING,
	YAHOO_SERVICE_GROUPRENAME,
	YAHOO_SERVICE_SYSMESSAGE = 0x14,
	YAHOO_SERVICE_PASSTHROUGH2 = 0x16,
	YAHOO_SERVICE_CONFINVITE = 0x18,
	YAHOO_SERVICE_CONFLOGON,
	YAHOO_SERVICE_CONFDECLINE,
	YAHOO_SERVICE_CONFLOGOFF,
	YAHOO_SERVICE_CONFADDINVITE,
	YAHOO_SERVICE_CONFMSG,
	YAHOO_SERVICE_CHATLOGON,
	YAHOO_SERVICE_CHATLOGOFF,
	YAHOO_SERVICE_CHATMSG = 0x20,
	YAHOO_SERVICE_GAMELOGON = 0x28,
	YAHOO_SERVICE_GAMELOGOFF,
	YAHOO_SERVICE_GAMEMSG = 0x2a,
	YAHOO_SERVICE_FILETRANSFER = 0x46,
	YAHOO_SERVICE_NOTIFY = 0x4B,
	YAHOO_SERVICE_AUTHRESP = 0x54,
	YAHOO_SERVICE_LIST = 0x55,
	YAHOO_SERVICE_AUTH = 0x57,
	YAHOO_SERVICE_ADDBUDDY = 0x83,
	YAHOO_SERVICE_REMBUDDY = 0x84
};

enum yahoo_status {
	YAHOO_STATUS_AVAILABLE = 0,
	YAHOO_STATUS_BRB,
	YAHOO_STATUS_BUSY,
	YAHOO_STATUS_NOTATHOME,
	YAHOO_STATUS_NOTATDESK,
	YAHOO_STATUS_NOTINOFFICE,
	YAHOO_STATUS_ONPHONE,
	YAHOO_STATUS_ONVACATION,
	YAHOO_STATUS_OUTTOLUNCH,
	YAHOO_STATUS_STEPPEDOUT,
	YAHOO_STATUS_INVISIBLE = 12,
	YAHOO_STATUS_CUSTOM = 99,
	YAHOO_STATUS_IDLE = 999,
	YAHOO_STATUS_OFFLINE = 0x5a55aa56, /* don't ask */
	YAHOO_STATUS_TYPING = 0x16
};
#define YAHOO_STATUS_GAME 0x2 /* Games don't fit into the regular status model */

#define YAHOO_PAGER_HOST "scs.yahoo.com"
#define YAHOO_PAGER_PORT 5050

/*
  Function Prototypes
*/
result yahoo_phandler(instance id, dpacket dp, void *arg);
struct yahoo_data *yahoo_get_session(char *who, jpacket jp, yahoo_instance yi);
void yahoo_remove_session(jpacket jp, yahoo_instance yi);
void yahoo_set_jabber_presence(struct yahoo_data *, char *contact_name, int state, char *);
void yahoo_pending(mio m, int flag, void *arg, char *buf, int len);
void yahoo_act_id(struct yahoo_data *, char *);
void yahoo_got_connected(struct yahoo_data *);
void yahoo_set_away(struct yahoo_data *, int state, char *msg);
xmlnode yahoo_xdb_get(yahoo_instance yi, char *host, jid owner);
int yahoo_xdb_set(yahoo_instance yi, char *host, jid owner, xmlnode data);
void yahoo_xdb_convert(yahoo_instance yi, char *user, jid nid);
void yahoo_set_jabber_buddy(struct yahoo_data *, char *contact_name, char *group);
void yahoo_close(struct yahoo_data *);
int yahoo_send_im(struct yahoo_data *, char *who, char *what, int len, int flags);
void yahoo_send_jabber_message(struct yahoo_data *, char *type, char *from, char *msg, int isUTF8);
void yahoo_transport_packets(jpacket jp);
int yahoo_get_session_connection_state(jpacket jp);
char *yahoo_get_status_string(enum yahoo_status a);
void yahoo_new_session(char *who, jpacket jp, yahoo_instance yi);
void yahoo_update_session_state(struct yahoo_data *, int state, char *msg);
void yahoo_debug_sessions(xht user);
void yahoo_remove_buddy(struct yahoo_data *yd, char *who, char *group);
void yahoo_add_buddy(struct yahoo_data *yd, char *who, char *group);
void yahoo_remove_session_yd(struct yahoo_data *yd);
int yahoo_get_registration_required(jpacket jp);
void yahoo_transport_presence_offline(struct yahoo_data *yd);
void yahoo_transport_presence_online(struct yahoo_data *yd);
void yahoo_stats(jpacket jp);
// void _yahoo_get_hash(xht hash, char *what);
void yahoo_unsubscribe_contacts(struct yahoo_data *);
void yahoo_unavailable_contacts(struct yahoo_data *yd);
void yahoo_unavailable_contact(xht ht, const char *key, void *data, void *arg);
void yahoo_send_jabber_composing_start(struct yahoo_data *yd, char *from);
void yahoo_send_jabber_composing_stop(struct yahoo_data *yd, char *from);
void yahoo_send_jabber_mail_notify(struct yahoo_data *yd, int count, char *from, char *subj);
void _jabberd_shutdown(void);
void yahoo_read_data(void *td);
void yahoo_transport_shutdown(void *arg);
#ifdef _JCOMP
void yahoo_jabber_handler(void *arg, void *user_data);
#else
void yahoo_jabber_handler(void *arg);
#endif

#define yahoo_deliver(ti, x) {xmlnode_hide_attrib(x,"origfrom"); deliver(dpacket_new(x),ti);}
