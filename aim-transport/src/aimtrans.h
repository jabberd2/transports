/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "jabberd.h"
#include "aim.h"
#include <sys/utsname.h>
#include <iconv.h>

#ifndef INCL_AIMTRANS_H
#define INCL_AIMTRANS_H

#ifdef __cplusplus
extern "C" {
#endif

#define AT_TRANS_VERSION "0.9"
#define AT_NS_AUTH "aimtrans:data"
#define AT_NS_ROSTER "aimtrans:roster"
#define NS_DISCO_ITEMS "http://jabber.org/protocol/disco#items"
#define NS_DISCO_INFO "http://jabber.org/protocol/disco#info"

typedef struct
{
    instance i;
    xdbcache xc;
    char *aimbinarydir;
    time_t start_time;
    xmlnode vcard;
    pth_mutex_t buddies_mutex;
    xht pending__buddies;
    xht session__list;
    xht iq__callbacks;
    int offset, read_len;
    char *modname;
    char *send_buf;
} *ati, _ati;

typedef struct 
{
    pth_message_t head; /* the standard pth message header */
    jpacket p;
} _jpq, *jpq;

typedef struct
{
    pool p;
    jpacket jp;
    ati ti;
} *at_mtq_data, _at_mtq_data;

typedef struct at_login_struct 
{
    char *user;
    char *pass;
    jid from;
    jid to;
    jpacket jp;
} _at_login, *at_login;

typedef struct at_buddy_struct 
{
    jid full;
    /* Expand in here to cache info from the server */
    time_t login_time;
    u_short idle_time;
    int is_away;
	xmlnode last;
    int iconlen;
    unsigned short iconsum;
    time_t iconstamp;
    unsigned char *icon;
    time_t sawawaymsg; /* timestamp of the last 'away' msg seen by aim user */
    time_t lastactivity;/*last time aim user sent an im */
} _at_buddy, *at_buddy;

/* Sessions */
typedef struct sessions_struct
{
    ati ti;
    pth_t tid;
    pth_msgport_t mp_to;
    jid cur;
    jid from;
    aim_session_t *ass;
    mio m;
    pool p;
    int exit_flag;
    int loggedin;
    char *profile;
    char *screenname;
    char *password;
    char *status;
    int away;
    time_t awaysetat;
    int icq;
    ppdb at_PPDB;
    xht buddies;

    jpacket icq_vcard_response;
} _at_session, *at_session;

typedef struct at_mio_data_struct
{
    pool p;
    aim_conn_t *conn;
    at_session s;
    aim_session_t *ass;
} _at_mio, *at_mio;

static char *msgerrreasons[] = {
  "Invalid error",
  "Invalid SNAC",
  "Rate to host",
  "Rate to client",
  "Not logged on",
  "Service unavailable",
  "Service not defined",
  "Obsolete SNAC",
  "Not supported by host",
  "Not supported by client",
  "Refused by client",
  "Reply too big",
  "Responses lost",
  "Request denied",
  "Busted SNAC payload",
  "Insufficient rights",
  "In local permit/deny",
  "Too evil (sender)",
  "Too evil (receiver)",
  "User temporarily unavailable",
  "No match",
  "List overflow",
  "Request ambiguous",
  "Queue full",
  "Not while on AOL"};
static int msgerrreasonslen = 25;

/* auth.c */
/* void at_sleeper_main(void *arg); */
int at_auth_user(ati ti, jpacket jp);
void at_auth_subscribe(ati ti, jpacket jp);

/* buddies.c */
void _at_buddies_unsubscribe(xht ht, const char *key, void *data, void * arg);
void at_buddy_find_stale(ati ti);
char *at_buddy_buildlist(at_session s, jid from);
int at_buddy_subscribe(ati ti, jpacket jp);
int at_buddy_add(ati ti, jpacket jp);
int at_parse_oncoming(aim_session_t *ass, 
                      aim_frame_t *command, ...);
int at_parse_offgoing(aim_session_t *ass, 
                      aim_frame_t *command, ...);
int at_parse_evilnotify(aim_session_t *sess, 
                              aim_frame_t *command, ...);
int at_parse_userinfo(aim_session_t *sess, 
                            aim_frame_t *command, ...);
int at_parse_icq_simpleinfo(aim_session_t *sess, 
                            aim_frame_t *command, ...);
int at_parse_locerr(aim_session_t *sess, 
                          aim_frame_t *command, ...);

/* charset.c */
extern iconv_t fromutf8, toutf8;
char *it_convert_utf82windows(pool p, const char *utf8_str);
char *it_convert_windows2utf8(pool p, const char *windows_str);

/* external.c */
void at_init();

/* iq.c */
typedef int (*iqcb)(ati ti, jpacket jp);

int at_register_iqns(ati ti, const char *ns, iqcb cb);
int at_run_iqcb(ati ti, const char *ns, jpacket jp);
void at_init_iqcbs(ati ti);

/* iq_cb.c */
int at_iq_vcard(ati ti, jpacket jp);
int at_iq_last(ati ti, jpacket jp);
int at_iq_gateway(ati ti, jpacket jp);
int at_iq_time(ati ti, jpacket jp);
int at_iq_version(ati ti, jpacket jp);
int at_iq_search(ati ti, jpacket jp);
int at_iq_size(ati ti, jpacket jp);
int at_iq_browse(ati ti, jpacket jp);
int at_iq_disco_items(ati ti, jpacket jp);
int at_iq_disco_info(ati ti, jpacket jp);

/* messages.c */
int at_parse_incoming_im(aim_session_t *ass, 
                         aim_frame_t *command, ...);
int at_offlinemsg(aim_session_t *sess, 
                  aim_frame_t *command, ...);
int at_offlinemsgdone(aim_session_t *sess, 
                      aim_frame_t *command, ...);
int at_icq_smsresponse(aim_session_t *sess, 
                       aim_frame_t *command, ...);
int at_parse_misses(aim_session_t *sess, 
                    aim_frame_t *command, ...);
int at_parse_msgerr(aim_session_t *sess, 
                    aim_frame_t *command, ...);

/* parser.c */
void at_parse_packet(void *arg);

/* pres.c */
int at_server_pres(ati ti, jpacket jp);
int at_session_pres(at_session s, jpacket jp);
void at_send_buddy_presence(at_session s, char *user);

/* register.c */
int at_register(ati ti, jpacket jp);

/* s10n.c */
int at_server_s10n(ati ti, jpacket jp);
int at_session_s10n(at_session s, jpacket jp);

/* sessions.c */
int at_parse_login(aim_session_t *sess, 
                   aim_frame_t *command, ...);
int at_conninitdone_bos(aim_session_t *sess, 
                        aim_frame_t *command, ...);
int at_conninitdone_admin(aim_session_t *sess, 
                          aim_frame_t *command, ...);
int at_parse_authresp(aim_session_t *sess, 
                      aim_frame_t *command, ...);
int at_handleredirect(aim_session_t *sess,
                      aim_frame_t *command, ...);
int at_parse_ratechange(aim_session_t *sess, 
                              aim_frame_t *command, ...);
int at_parse_motd(aim_session_t *sess, 
                  aim_frame_t *command, ...);
int at_parse_connerr(aim_session_t *sess, 
                     aim_frame_t *command, ...);
int at_flapversion(aim_session_t *sess, 
                   aim_frame_t *command, ...);
int at_conncomplete(aim_session_t *sess, 
                    aim_frame_t *command, ...);
ssize_t at_session_handle_read(mio m, void* buf, size_t count);
ssize_t at_session_handle_write(mio m, const void* buf, size_t count);
void at_handle_mio(mio m, int flags, void *arg, char *buffer, int bufsz);
at_session at_session_create(ati ti, xmlnode aim_data, jpacket jp);
void *at_session_main(void *arg);
void at_aim_session_parser(at_session s, jpacket jp);
void at_session_deliver(at_session s, xmlnode x, jid from);
void at_session_end(at_session s);
void at_session_unlink(at_session s);
void at_session_unlink_buddies(at_session s);
void at_session_buddy_unlink(at_session s, char *user);
at_session at_session_find_by_jid(ati ti, jid j);
int at_flapversion(aim_session_t *sess, 
                aim_frame_t *command, ...);
int at_conncomplete(aim_session_t *sess, 
                    aim_frame_t *command, ...);
aim_conn_t *_aim_select(aim_session_t *ass, 
                               pth_event_t ev, 
                               int *status);

/* utils.c */
char *strip_html(char * text,pool p);
void at_psend(pth_msgport_t mp, jpacket p);
char *str_to_UTF8(pool p,unsigned char *in);
unsigned char *UTF8_to_str(pool p,unsigned char *in);
unsigned char *iconvstr(pool p, iconv_t code, unsigned char *in);
char *at_normalize(char *s);
int at_xdb_set(ati ti, char *host, jid owner, xmlnode data, char *ns);
xmlnode at_xdb_get(ati ti, jid owner, char *ns);
void at_xdb_convert(ati ti, char *user, jid nid);

/* unknown.c */
void *at_unknown_main(void *arg);

/* icq.c */
faim_export int aim_icq_sendsms(aim_session_t *sess, const char *dest, const char *body);

#define at_deliver(ti, x) {xmlnode_hide_attrib(x,"origfrom"); deliver(dpacket_new(x),ti->i);}

#ifdef __cplusplus
}
#endif

#endif /* INCL_AIMTRANS_H */
