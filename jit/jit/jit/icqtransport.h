/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

#include "jabberd.h"


#include <iconv.h>

#ifdef SUNOS
#include <limits.h>
#endif

#include "utils/polish.h"

#define MOD_VERSION "1.1.7"
#define DEFAULT_CHARSET "iso-8859-1"

typedef unsigned long UIN_t;
#define SMS_CONTACT ULONG_MAX

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_AUTH_HOSTS 5

typedef struct session_st *session, _session;
typedef struct session_ref_st *session_ref, _session_ref;

/** ICQ contact status */
typedef enum
{
    ICQ_STATUS_NOT_IN_LIST=0,
    ICQ_STATUS_OFFLINE=1,
    ICQ_STATUS_ONLINE=2,
    ICQ_STATUS_AWAY=3,
    ICQ_STATUS_DND=4,
    ICQ_STATUS_NA=5,
    ICQ_STATUS_OCCUPIED=6,
    ICQ_STATUS_FREE_CHAT=7,
    ICQ_STATUS_INVISIBLE=8
} icqstatus;


/** ICQ transport instance structure */
typedef struct
{
    /** jabberd instance struct */
    instance i;
    /** needed for XDB lookup routines */
    xdbcache xc;
    /** the transport's vcard */
    xmlnode vcard;
    /** ? */
  //    xmlnode admin;
    SEM_VAR sessions_sem;
    /** xhash list with transport sessions */
    wpxht sessions;
    wpxht sessions_alt;
    int sessions_count;
    char *registration_instructions;
    char *search_instructions;
    /** user count file name, default "icqcount" */
    char *count_file;
    char *auth_hosts[MAX_AUTH_HOSTS];
    int auth_ports[MAX_AUTH_HOSTS];
    int auth_hosts_count;
    /** charset to use on ICQ side. Default is windows-1252. */
    char *charset;
    int reconnect;
    int session_timeout;
    /** FQDN of SMS component */
    char *sms_id; 
    /** Status of SMS contacts */
    icqstatus sms_show;
    /** Away message of SMS contacts */
    char *sms_status;
    /** Name of the SMS node for browsing and discovery */
    char *sms_name;
    /** use "chat" as standard message type? */
    int msg_chat;
    time_t start;
    /** shutdown flag */
    int shutdown;
    /** webaware flag */
    unsigned char web_aware;  
    /** no:x:data in forms */
    unsigned char no_x_data;  
    /** use own roster flag */
    unsigned char own_roster; 
    /** don't use main jabber roster flag */
    unsigned char no_jabber_roster;
    /** separated thread queue for unknown packets */
    mtq q; 
} *iti, _iti;

/** Search structure */
typedef struct
{
    char *nick;
    char *first;
    char *last;
    char *email;
    char *email2;
    char *old_email;
    char *city;
    char *state;
    char *phone;
    char *fax;
    char *street;
    char *cellphone;
    unsigned long zipcode;
    unsigned short country_code;
    int tz_offset;
    int auth;
    icqstatus status;
    int web_aware;
    int hide_ip;
} meta_gen;

typedef void (*meta_info_cb)(session s, unsigned short type, void *data, void *arg);
typedef void (*meta_search_cb)(session s, UIN_t uin, meta_gen *res, void *arg);

typedef struct
{
    pool p;
    void *cb;
    void *arg;
} *pendmeta, _pendmeta;

typedef struct
{
    jpacket jp;
    void *arg;
} *vcard_wait, _vcard_wait;

/** Contacts are stored in a linear list. */
typedef struct contact_st
{
    pool p;
    session s;
    /** UIN if standard contact, const SMS_CONTACT if SMS contact */
    UIN_t uin;
    /** User part of JID if SMS contact */
    char * sms;
    icqstatus status;
    int asking;
    int use_chat_msg_type;
    struct contact_st *next;
} *contact, _contact;

/** session type */
typedef enum {stype_normal, stype_register} stype;

/** one element in a linear list */
typedef struct elemqueue
{
  void * elem;
  struct elemqueue *next;
} _queue_elem, *queue_elem;

/** Append new element (of type queue_elem) to linear list. New element will be last in list.
 * @param first The first element in the list
 * @param last The last element in the list
 * @param elem The element to be appended */
#define QUEUE_PUT(first,last,elem) \
      if ((first) == NULL) { \
    (first) = (elem); \
    (last) = (elem);  } \
      else { \
    (last)->next = (elem); \
    (last) = (elem);  } 

/** Get first element from linear list. Remove this element from list.
 * @param first The first element in the list
 * @param last The last element in the list */
#define QUEUE_GET(first,last) \
  ( \
   ({ \
     queue_elem __queue = (first); \
     if ((last) == (first)) { \
       (first) = NULL; \
       (last) = NULL; } \
     else { \
       (first) = (first)->next; } \
     (queue_elem)__queue; \
   })) \

#define MAX_STATUS_TEXT 100

/** Transport session. One session per transport user online. */
struct session_st
{
    WPHASH_BUCKET;
    pool p;
    /** user JID (without resource) */
    jid id;
    /** user JID (with resource) */
    jid orgid;
    /** transport's JID */
    jid from;
    /** managed thread queue (for inter-thread communication ;-) */
    mtq q;
    /** our main transport instance */
    iti ti;
    /** session type (normal or registration) */
    stype type;
    UIN_t uin;
    char *passwd;
    char status_text[MAX_STATUS_TEXT+1];
    /** presence proxy data base */
    ppdb p_db;

    /** packet queue (linear list) */
    queue_elem queue;      /* pointer to first element in list */
    queue_elem queue_last; /* pointer to last element in list */

    /** managed I/O (TCP sockets) */
    volatile mio s_mio;

    /** for searching, only one search */
    pendmeta pend_search;
    /** for getting vcard, only one get at the same time */
    vcard_wait vcard_get;

    /** our status */
    icqstatus status;
    /** flag that we are connected and authorized */
    volatile int connected;
    /** exit_flag means that session is closing */
    volatile int exit_flag;
    /** linear list with contacts */
    contact contacts;
    /** contacts count */
    int contact_count;
    /** time when session started */
    int start_time;
    /** last time a packet arrived */
    int last_time;
    /** flag that user can reconnect */
    unsigned char reconnect;
    /** reconnect count */
    unsigned char reconnect_count;
    /** web presence */
    unsigned char web_aware;
    /** flag that on exit save user roster */
    unsigned char roster_changed;
    /** reference>0 means that session memory is used and can't be free */
    volatile int reference_count;
    /** pointer to "our" client */
    volatile void * client;
};

struct session_ref_st
{
    WPHASH_BUCKET;
    session s;
};

void icqtrans(instance i, xmlnode x);
result it_receive(instance i, dpacket d, void *arg);
session it_session_create(iti ti, jpacket jp);
void it_session_jpacket(void *arg);
void it_session_confirmed(session s);
void it_session_connected(void *arg);
void it_session_end(session s);
void it_session_error(session s, terror e);
void it_sessions_end(wpxht h, const char *key, void *val, void *arg);

void it_unknown(iti ti, jpacket jp);
void it_unknown_bounce(void *arg);
void it_message(session s, jpacket jp);
void it_vcard(session s, jpacket jp);;
void it_iq(session s, jpacket jp);
void it_presence(session s, jpacket jp);
void it_s10n(session s, jpacket jp);
void it_s10n_go(session s, jpacket jp, UIN_t uin);
void it_message_go(session s, jpacket jp, UIN_t uin);

/* IQ */
void it_iq_version(iti ti, jpacket jp);
void it_iq_vcard_server(iti ti, jpacket jp);
void it_iq_vcard(session s, jpacket jp);
void it_iq_last(session s, jpacket jp);
void it_iq_last_server(iti ti, jpacket jp);
void it_iq_time(iti ti, jpacket jp);
void it_iq_reg_get(session s, jpacket jp);
void it_iq_reg_remove(session s, jpacket jp);
void it_iq_search_get(session s, jpacket jp);
void it_iq_search_set(session s, jpacket jp);
void it_iq_gateway_get(session s, jpacket jp);
void it_iq_gateway_set(session s, jpacket jp);
void it_iq_browse_server(iti ti, jpacket jp);
void it_iq_browse_user(session s, jpacket jp);

void it_iq_disco_items_server(iti ti, jpacket jp);
void it_iq_disco_info_server(iti ti, jpacket jp);
void it_iq_disco_items_user(session s, jpacket jp);
void it_iq_disco_info_user(session s, jpacket jp);


void it_save_contacts(session s);
contact it_unknown_contact_add(session s,char *user,UIN_t uin);
void it_contact_load_roster(session s);
contact it_contact_get(session s, UIN_t uin);
void it_sms_presence(session s,int status);
contact it_sms_get(session s, char * id);
jid it_sms2jid(pool p, char * id, char *server);
contact it_contact_add(session s, UIN_t uin);
contact it_sms_add(session s, char *id);
void it_contact_remove(contact c);
//void it_contact_update_addr(contact c, unsigned long ip, unsigned short port, unsigned long real_ip, icqbyte force);
void it_contact_set_status(contact c, icqstatus show,char * status);
void it_contact_send_presence(contact c,char * status) ;
void it_contact_subscribe(contact c,const char * name);
void it_contact_unsubscribe(contact c);
void it_contact_subscribed(contact c, jpacket jp);
void it_contact_flush(contact c);
void it_contact_free(session s);

char *it_convert_utf82windows(pool p, const char *utf8_str);
char *it_convert_windows2utf8(pool p, const char *windows_str);
char *it_convert_ucs2utf8(pool p, unsigned int len, const char ucs2_str[]);

jid it_xdb_id(pool p, jid id, char *server);
void it_xdb_convert(iti ti, char *user, jid nid);
UIN_t it_strtouin(char *uin);
jid it_uin2jid(pool p, UIN_t uin, char *host);
const char * jit_status2fullinfo(icqstatus status);
icqstatus jit_show2status(const char *show);
const char * jit_status2show(icqstatus status);
//unsigned long it_status_to_bits(icqstatus status);
//icqstatus it_status_from_bits(unsigned long bits);
void it_delay(xmlnode x, char *ts);
int it_reg_set(session s, xmlnode reg);

void SendAuthRequest(contact c,char * what);
void SendRemoveContact(contact c);
void SendAuthGiven(contact c);
void SendAuthDenied(contact c);
void AddICQContact(contact c);
void SendUrl(session s,char * url,char * desc,UIN_t uin);
void SendMessage(session s,char * body,unsigned char chat,UIN_t uin);
void SendSMS(session s,char * body,char * nr);
void StartClient(session s);
void FetchServerBasedContactList(session s);
void EndClient(session s);
void SendStatus(session s);
void SessionCheck(session s);

#define it_jid2uin(x) it_strtouin(x->user)
#define it_deliver(ti, x) {xmlnode_hide_attrib(x,"origfrom"); deliver(dpacket_new(x),ti->i);}

/* xdata */
xmlnode xdata_create(xmlnode q, char *type);
xmlnode xdata_insert_field(xmlnode q, char *type, char *var, char *label, char *data);
xmlnode xdata_insert_node(xmlnode q, char *name);
xmlnode xdata_insert_option(xmlnode q, char *label, char *data);
int xdata_test(xmlnode q, char *type);
char *xdata_get_data(xmlnode q, char *name);
xmlnode xdata_convert(xmlnode q, char *ns);

#ifdef __cplusplus
}
#endif
