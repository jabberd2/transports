/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * libfaim code copyright 1998, 1999 Adam Fritzler <afritz@auk.cx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

//  #ifdef HAVE_CONFIG_H
//  #include "config.h"
//  #endif
//
//
//  #include <netdb.h>
//  #include <unistd.h>
//  #include <errno.h>
//  #include <netinet/in.h>
//  #include <arpa/inet.h>
//  #include <string.h>
//  #include <stdlib.h>
//  #include <stdio.h>
//  #include <time.h>
//  #include <sys/socket.h>
//  #include <sys/stat.h>
//  #include <ctype.h>
//  #include "multi.h"
//  #include "prpl.h"
//  #include "gaim.h"
//  #include "proxy.h"
#include "yahoo-transport.h"
#include "md5.h"
#include "gaim-sha.h"
#include "yahoo-auth.h"

extern char *yahoo_crypt(const char *, const char *);

//  #include "pixmaps/status-away.xpm"
//  #include "pixmaps/status-here.xpm"
//  #include "pixmaps/status-idle.xpm"
//  #include "pixmaps/status-game.xpm"
//
//  /* Yahoo Smilies go here */
//  #include "pixmaps/protocols/yahoo/yahoo_alien.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_angel.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_angry.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_bigsmile.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_blush.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_bye.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_clown.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_cow.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_cowboy.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_cry.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_devil.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_flower.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_ghost.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_glasses.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_laughloud.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_love.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_mean.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_neutral.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_ooooh.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_question.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_sad.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_sleep.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_smiley.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_sunglas.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_tongue.xpm"
//  #include "pixmaps/protocols/yahoo/yahoo_wink.xpm"
//
//  #define YAHOO_DEBUG
//
#define USEROPT_MAIL 0
//
#define USEROPT_PAGERHOST 3
//  #define YAHOO_PAGER_HOST "scs.yahoo.com"
#define USEROPT_PAGERPORT 4
//  #define YAHOO_PAGER_PORT 5050

#define YAHOO_PROTO_VER 0x000b

//  enum yahoo_service { /* these are easier to see in hex */
//  	YAHOO_SERVICE_LOGON = 1,
//  	YAHOO_SERVICE_LOGOFF,
//  	YAHOO_SERVICE_ISAWAY,
//  	YAHOO_SERVICE_ISBACK,
//  	YAHOO_SERVICE_IDLE, /* 5 (placemarker) */
//  	YAHOO_SERVICE_MESSAGE,
//  	YAHOO_SERVICE_IDACT,
//  	YAHOO_SERVICE_IDDEACT,
//  	YAHOO_SERVICE_MAILSTAT,
//  	YAHOO_SERVICE_USERSTAT, /* 0xa */
//  	YAHOO_SERVICE_NEWMAIL,
//  	YAHOO_SERVICE_CHATINVITE,
//  	YAHOO_SERVICE_CALENDAR,
//  	YAHOO_SERVICE_NEWPERSONALMAIL,
//  	YAHOO_SERVICE_NEWCONTACT,
//  	YAHOO_SERVICE_ADDIDENT, /* 0x10 */
//  	YAHOO_SERVICE_ADDIGNORE,
//  	YAHOO_SERVICE_PING,
//  	YAHOO_SERVICE_GROUPRENAME,
//  	YAHOO_SERVICE_SYSMESSAGE = 0x14,
//  	YAHOO_SERVICE_PASSTHROUGH2 = 0x16,
//  	YAHOO_SERVICE_CONFINVITE = 0x18,
//  	YAHOO_SERVICE_CONFLOGON,
//  	YAHOO_SERVICE_CONFDECLINE,
//  	YAHOO_SERVICE_CONFLOGOFF,
//  	YAHOO_SERVICE_CONFADDINVITE,
//  	YAHOO_SERVICE_CONFMSG,
//  	YAHOO_SERVICE_CHATLOGON,
//  	YAHOO_SERVICE_CHATLOGOFF,
//  	YAHOO_SERVICE_CHATMSG = 0x20,
//  	YAHOO_SERVICE_GAMELOGON = 0x28,
//  	YAHOO_SERVICE_GAMELOGOFF,
//  	YAHOO_SERVICE_GAMEMSG = 0x2a,
//  	YAHOO_SERVICE_FILETRANSFER = 0x46,
//  	YAHOO_SERVICE_NOTIFY = 0x4B,
//  	YAHOO_SERVICE_AUTHRESP = 0x54,
//  	YAHOO_SERVICE_LIST = 0x55,
//  	YAHOO_SERVICE_AUTH = 0x57,
//  	YAHOO_SERVICE_ADDBUDDY = 0x83,
//  	YAHOO_SERVICE_REMBUDDY = 0x84
//  };
//
//  enum yahoo_status {
//  	YAHOO_STATUS_AVAILABLE = 0,
//  	YAHOO_STATUS_BRB,
//  	YAHOO_STATUS_BUSY,
//  	YAHOO_STATUS_NOTATHOME,
//  	YAHOO_STATUS_NOTATDESK,
//  	YAHOO_STATUS_NOTINOFFICE,
//  	YAHOO_STATUS_ONPHONE,
//  	YAHOO_STATUS_ONVACATION,
//  	YAHOO_STATUS_OUTTOLUNCH,
//  	YAHOO_STATUS_STEPPEDOUT,
//  	YAHOO_STATUS_INVISIBLE = 12,
//  	YAHOO_STATUS_CUSTOM = 99,
//  	YAHOO_STATUS_IDLE = 999,
//  	YAHOO_STATUS_OFFLINE = 0x5a55aa56, /* don't ask */
//  	YAHOO_STATUS_TYPING = 0x16
//  };
//  #define YAHOO_STATUS_GAME 0x2 /* Games don't fit into the regular status model */
//
//  struct yahoo_data {
//  	int fd;
//  	guchar *rxqueue;
//  	int rxlen;
//  	GHashTable *hash;
//  	GHashTable *games;
//  	int current_status;
//  	gboolean logged_in;
//  };

struct yahoo_pair {
    int key;
    char *value;
};

struct yahoo_packet {
    guint16 service;
    guint32 status;
    guint32 id;
    GSList *hash;
};
/*
static char *yahoo_name() {
    return "Yahoo";
}
*/
#define YAHOO_PACKET_HDRLEN (4 + 2 + 2 + 2 + 2 + 4 + 4)

static char *normalize(const char *s) {
    static char buf[64];
    char tmp[64];
    int i, j;

    memset(tmp, 0, 64);
    strncpy(buf, s, sizeof(buf));
    for (i=0, j=0; buf[j]; i++, j++) {
        while (buf[j] == ' ')
            j++;
        tmp[i] = buf[j];
    }
    tmp[i] = '\0';

    g_strdown(tmp);
    memset(buf, 0, 64);
    g_snprintf(buf, sizeof(buf), "%s", tmp);

    return buf;
}

struct yahoo_packet *yahoo_packet_new(enum yahoo_service service, enum yahoo_status status, int id) {
    struct yahoo_packet *pkt = g_new0(struct yahoo_packet, 1);

    pkt->service = service;
    pkt->status = status;
    pkt->id = id;

    return pkt;
}

static void yahoo_packet_hash(struct yahoo_packet *pkt, int key, const char *value) {
    struct yahoo_pair *pair = g_new0(struct yahoo_pair, 1);
    pair->key = key;
    pair->value = g_strdup(value);
    pkt->hash = g_slist_append(pkt->hash, pair);
}

static int yahoo_packet_length(struct yahoo_packet *pkt) {
    GSList *l;

    int len = 0;
    char *fix;

    l = pkt->hash;
    while (l) {
        struct yahoo_pair *pair = l->data;
        int tmp = pair->key;
        do {
            tmp /= 10;
            len++;
        } while (tmp);
        len += 2;
	if (pair->value == NULL) {
	    fix = malloc(1);
	    *fix = '\0';
	    pair->value = fix;
	    log_debug(ZONE , " [YAHOO]: Corrected Key Now: %d tValue: %sn " , pair->key , pair->value);
	}
	len += pair->value == NULL ? 0 : strlen(pair->value);
        len += 2;
        l = l->next;
    }

    return len;
}

/* sometimes i wish prpls could #include things from other prpls. then i could just
 * use the routines from libfaim and not have to admit to knowing how they work. */
#define yahoo_put16(buf, data) ( \
(*(buf) = (u_char)((data)>>8)&0xff), \
(*((buf)+1) = (u_char)(data)&0xff),  \
2)
#define yahoo_get16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define yahoo_put32(buf, data) ( \
(*((buf)) = (u_char)((data)>>24)&0xff), \
(*((buf)+1) = (u_char)((data)>>16)&0xff), \
(*((buf)+2) = (u_char)((data)>>8)&0xff), \
(*((buf)+3) = (u_char)(data)&0xff), \
4)
#define yahoo_get32(buf) ((((*(buf))<<24)&0xff000000) + \
(((*((buf)+1))<<16)&0x00ff0000) + \
(((*((buf)+2))<< 8)&0x0000ff00) + \
(((*((buf)+3)    )&0x000000ff)))

static void yahoo_packet_read(struct yahoo_packet *pkt, guchar *data, int len) {
	int pos = 0;

	while (pos + 1 < len) {
		char key[64], *value = NULL, *esc;
		int accept;
		int x;

		struct yahoo_pair *pair = g_new0(struct yahoo_pair, 1);

		/* this is weird, and in one of the chat packets, and causes us
		 * think all the values are keys and all the keys are values after
		 * this point if we don't handle it */
		if (data[pos] == '\0') {
			while (pos + 1 < len) {
				if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
					break;
				pos++;
			}
			pos += 2;
			g_free(pair);
			continue;
		}

		x = 0;
		while (pos + 1 < len) {
			if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
				break;
			if (x >= sizeof(key)-1) {
				x++;
				pos++;
				continue;
			}
			key[x++] = data[pos++];
		}
		if (x >= sizeof(key)-1) {
			x = 0;
		}
		key[x] = 0;
		pos += 2;
		pair->key = strtol(key, NULL, 10);
		accept = x; /* if x is 0 there was no key, so don't accept it */

		if (len - pos + 1 <= 0) {
			/* Truncated. Garbage or something. */
			accept = 0;
		}

		if (accept) {
			value = g_malloc(len - pos + 1);
			x = 0;
			while (pos + 1 < len) {
				if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
					break;
				value[x++] = data[pos++];
			}
			value[x] = 0;
			pair->value = g_strdup(value);
			g_free(value);
			pkt->hash = g_slist_append(pkt->hash, pair);
			esc = g_strescape(pair->value);
			log_debug(ZONE, "Read Key: %d  \tValue: %s\n", pair->key, esc);
			g_free(esc);
		} else {
			g_free(pair);
		}
		pos += 2;

		/* Skip over garbage we've noticed in the mail notifications */
		if (data[0] == '9' && data[pos] == 0x01)
			pos++;
	}
}

static void yahoo_packet_write(struct yahoo_packet *pkt, guchar *data) {
    GSList *l = pkt->hash;
    int pos = 0;

    while (l) {
        struct yahoo_pair *pair = l->data;
        guchar buf[100];

        g_snprintf(buf, sizeof(buf), "%d", pair->key);
        strcpy(data + pos, buf);
        pos += strlen(buf);
        data[pos++] = 0xc0;
        data[pos++] = 0x80;

        strcpy(data + pos, pair->value);
        pos += strlen(pair->value);
        data[pos++] = 0xc0;
        data[pos++] = 0x80;
        log_debug(ZONE, "[YAHOO]: Write Key: %d  \tValue: %s\n", pair->key, pair->value);
        l = l->next;
    }
}

static void yahoo_packet_dump(guchar *data, int len) {
#ifdef YAHOO_DEBUG
    int i;
    log_debug(ZONE, "[YPKT]: start");
    for (i = 0; i + 1 < len; i += 2) {
        log_debug(ZONE, "[YPKT]: %02x", data[i]);
        log_debug(ZONE, "[YPKT]: %02x ", data[i+1]);
    }
    if (i < len)
        log_debug(ZONE, "[YPKT]: %02x", data[i]);
    log_debug(ZONE, "[YPKT]: --------");
    for (i = 0; i < len; i++) {
        if (isprint(data[i])) {
            log_debug(ZONE, "[YPKT]: %c ", data[i]);
        } else {
            log_debug(ZONE, "[YPKT]: . ");
        }
    }
    log_debug(ZONE, "[YPKT]: end");
#endif
}

static int yahoo_send_packet(struct yahoo_data *yd, struct yahoo_packet *pkt) {
    int pktlen = yahoo_packet_length(pkt);
    int len = YAHOO_PACKET_HDRLEN + pktlen;

    guchar *data;
    int pos = 0;

    if (yd->fd->fd < 0)
        return -1;

    data = g_malloc0(len + 1);

    memcpy(data + pos, "YMSG", 4); pos += 4;
    pos += yahoo_put16(data + pos, YAHOO_PROTO_VER);
    pos += yahoo_put16(data + pos, 0x0000);
    pos += yahoo_put16(data + pos, pktlen);
    pos += yahoo_put16(data + pos, pkt->service);
    pos += yahoo_put32(data + pos, pkt->status);
    pos += yahoo_put32(data + pos, pkt->id);

    yahoo_packet_write(pkt, data + pos);

    yahoo_packet_dump(data, len);
    log_debug(ZONE, "[YAHOO]: Writing %d bytes to Yahoo! (fd=%d) state=%d", len, yd->fd->fd, yd->connection_state);
    mio_write(yd->fd, NULL, data, len);
    yd->yi->stats->bytes_out += len;
    g_free(data);

    return len;
}

static void yahoo_packet_free(struct yahoo_packet *pkt) {
    while (pkt->hash) {
        struct yahoo_pair *pair = pkt->hash->data;
        g_free(pair->value);
        g_free(pair);
        pkt->hash = g_slist_remove(pkt->hash, pair);
    }
    g_free(pkt);
}

static void yahoo_process_status(struct yahoo_data *yd, struct yahoo_packet *pkt) {
    GSList *l = pkt->hash;
    char *name = NULL;
    int state = 0;
    int away = 0;
    // int gamestate = 0;
    char *msg = NULL;

    while (l) {
        struct yahoo_pair *pair = l->data;

        log_debug(ZONE, "[YAHOO]: Process Status: %d '%s'\n", pair->key, pair->value);

        switch (pair->key) {
        case 0: /* we won't actually do anything with this */
            break;
        case 1: /* we don't get the full buddy list here. */
            if (!yd->logged_in) {
                // FUNC				account_online(gc);
                // FUNC				serv_finish_login(gc);
                g_snprintf(yd->displayname, sizeof(yd->displayname), "%s", pair->value);
                yd->logged_in = TRUE;
                log_notice(ZONE, "[YAHOO]: '%s' Logged in as '%s' (fd=%d)", jid_full(yd->me), yd->username, yd->fd->fd);
                yahoo_update_session_state(yd, 0, "yahoo_process_status");

                /* this requests the list. i have a feeling that this is very evil
                 *
                 * scs.yahoo.com sends you the list before this packet without  it being 
                 * requested
                 *
                 * do_import(gc, NULL);
                 * newpkt = yahoo_packet_new(YAHOO_SERVICE_LIST, YAHOO_STATUS_OFFLINE, 0);
                 * yahoo_send_packet(yd, newpkt);
                 * yahoo_packet_free(newpkt);
                 */

            }
            break;
        case 8: /* how many online buddies we have */
            break;
        case 7: /* the current buddy */
            name = pair->value;
            break;
        case 10: /* state */
            state = strtol(pair->value, NULL, 10);
            break;
        case 19: /* custom message */
            msg = pair->value;
            break;
        case 11: /* i didn't know what this was in the old protocol either */
            break;
        case 17: /* in chat? */
            break;
        case 13: /* in pager? */
            if (pkt->service == YAHOO_SERVICE_LOGOFF ||
                        strtol(pair->value, NULL, 10) == 0) {
                yahoo_set_jabber_presence(yd, name, YAHOO_STATUS_OFFLINE, NULL);
                break;
            }
            //       if (g_hash_table_lookup(yd->games, name))
            //          gamestate = YAHOO_STATUS_GAME;
            if (state == YAHOO_STATUS_CUSTOM) {
                if (msg)
		    yahoo_set_jabber_presence(yd, name,
		        away ? YAHOO_STATUS_BRB : YAHOO_STATUS_AVAILABLE,
		        msg);
                else
		    yahoo_set_jabber_presence(yd, name,
		        away ? YAHOO_STATUS_BRB : YAHOO_STATUS_AVAILABLE,
		        yahoo_get_status_string(state));
            } else {
                if (msg)
		    yahoo_set_jabber_presence(yd, name, state, msg);
                else
                    yahoo_set_jabber_presence(yd, name, state, yahoo_get_status_string(state));
            }
            //       if (state == YAHOO_STATUS_CUSTOM) {
            //          gpointer val = g_hash_table_lookup(yd->hash, name);
            //          if (val) {
            //             g_free(val);
            //             g_hash_table_insert(yd->hash, name,
            //                                 msg ? g_strdup(msg) : g_malloc0(1));
            //          } else
            //             g_hash_table_insert(yd->hash, g_strdup(name),
            //                                 msg ? g_strdup(msg) : g_malloc0(1));
            //       }
            msg = NULL;
            break;
	case 47: /* CUSTOM state away indicator */
	    away = strtol(pair->value , NULL , 10);
	    break;
        case 60: /* no clue */
            break;
        case 16: /* Custom error message */
            log_debug(ZONE, "[YAHOO]: Error Message: %s\n", pair->value);
            break;
        default:
            log_debug(ZONE, "[YAHOO]: unknown status key %d\n", pair->key);
            break;
        }

        l = l->next;
    }
}

static void yahoo_process_list(struct yahoo_data *yd, struct yahoo_packet *pkt) {
    GSList *l = pkt->hash;
    // gboolean export = FALSE;

    while (l) {
        char **lines;
        char **split;
        char **buddies;
        char **tmp, **bud;

        struct yahoo_pair *pair = l->data;
        l = l->next;

        if (pair->key != 87)
            continue;

        // FUNC		do_import(gc, NULL);
        lines = g_strsplit(pair->value, "\n", -1);
        for (tmp = lines; *tmp; tmp++) {
            split = g_strsplit(*tmp, ":", 2);
            if (!split)
                continue;
            if (!split[0] || !split[1]) {
                g_strfreev(split);
                continue;
            }
            buddies = g_strsplit(split[1], ",", -1);
            for (bud = buddies; bud && *bud; bud++)
                // FUNC				if (!find_buddy(gc, *bud)) {
                yahoo_set_jabber_buddy(yd, *bud, split[0]);
            //					export = TRUE;
            //				}
            g_strfreev(buddies);
            g_strfreev(split);
        }
        g_strfreev(lines);
    }

    //	if (export)
    // FUNC	 	do_export(gc);
}

static void yahoo_process_notify(struct yahoo_data *yd, struct yahoo_packet *pkt) {
    char *msg = NULL;
    char *from = NULL;
    char *stat = NULL;
    char *game = NULL;
    GSList *l = pkt->hash;

    while (l) {
        struct yahoo_pair *pair = l->data;
        if (pair->key == 4)
            from = pair->value;
        if (pair->key == 49)
            msg = pair->value;
        if (pair->key == 13)
            stat = pair->value;
        if (pair->key == 14)
            game = pair->value;
        l = l->next;
    }

    if (!g_strncasecmp(msg, "TYPING", strlen("TYPING"))) {
        if (*stat == '1')
          yahoo_send_jabber_composing_start(yd, from);
        else
          yahoo_send_jabber_composing_stop(yd, from);
    }
}

static void yahoo_process_message(struct yahoo_data *yd, struct yahoo_packet *pkt) {
    char *msg = NULL;
    char *from = NULL;
    time_t tm = time(NULL);
    int isUTF8 = 0;
    GSList *l = pkt->hash;

    while (l) {
        struct yahoo_pair *pair = l->data;
        if (pair->key == 4)
            from = pair->value;
        if (pair->key == 97) {
            if (strncasecmp(pair->value, "1", 1) == 0)
               isUTF8 = 1;
        }
        if (pair->key == 14)
            msg = pair->value;
        if (pair->key == 15)
            tm = strtol(pair->value, NULL, 10);
        l = l->next;
    }

    if (pkt->status <= 1 || pkt->status == 5) {
        char *m;
        int i, j;
        //		strip_linefeed(msg);
        m = msg;
        for (i = 0, j = 0; m[i]; i++) {
            if (m[i] == 033) {
                while (m[i] && (m[i] != 'm'))
                    i++;
                if (!m[i])
                    i--;
                continue;
            }
            msg[j++] = m[i];
        }
        msg[j] = 0;
        yahoo_send_jabber_message(yd, "chat", from, msg, isUTF8);
    } else if (pkt->status == 2) {
        // FUNC		do_error_dialog(_("Your message did not get sent."), _("Gaim - Error"));
    }
}


static void yahoo_process_contact(struct yahoo_data *yd, struct yahoo_packet *pkt) {
    char *id = NULL;
    char *who = NULL;
    char *msg = NULL;
    char *name = NULL;
    int state = YAHOO_STATUS_AVAILABLE;
    int online = FALSE;

    GSList *l = pkt->hash;

    while (l) {
        struct yahoo_pair *pair = l->data;
        if (pair->key == 1)
            id = pair->value;
        else if (pair->key == 3)
            who = pair->value;
        else if (pair->key == 14)
            msg = pair->value;
        else if (pair->key == 7)
            name = pair->value;
        else if (pair->key == 10)
            state = strtol(pair->value, NULL, 10);
        else if (pair->key == 13)
            online = strtol(pair->value, NULL, 10);
        l = l->next;
    }

    if (id)
        yahoo_set_jabber_buddy(yd, who, "Buddies");
    // FUNC		show_got_added(gc, id, who, NULL, msg);
    if (name) {
        //		if (state == YAHOO_STATUS_AVAILABLE)
        //			serv_got_update(gc, name, 1, 0, 0, 0, 0, 0);
        //		else if (state == YAHOO_STATUS_IDLE)
        //			serv_got_update(gc, name, 1, 0, 0, time(NULL) - 600, (state << 2), 0);
        //		else
        //			serv_got_update(gc, name, 1, 0, 0, 0, (state << 2) | UC_UNAVAILABLE, 0);
        //		if (state == YAHOO_STATUS_CUSTOM) {
        //			gpointer val = g_hash_table_lookup(yd->hash, name);
        //			if (val) {
        //				g_free(val);
        //				g_hash_table_insert(yd->hash, name,
        //						msg ? g_strdup(msg) : g_malloc0(1));
        //			} else
        //				g_hash_table_insert(yd->hash, g_strdup(name),
        //						msg ? g_strdup(msg) : g_malloc0(1));
        //		}
    }
}

static void yahoo_process_mail(struct yahoo_data *yd, struct yahoo_packet *pkt) {
    char *who = NULL;
    char *email = NULL;
    char *subj = NULL;
    int count = 0;
    GSList *l = pkt->hash;

    while (l) {
        struct yahoo_pair *pair = l->data;
        if (pair->key == 9)
            count = strtol(pair->value, NULL, 10);
        else if (pair->key == 43)
            who = pair->value;
        else if (pair->key == 42)
            email = pair->value;
        else if (pair->key == 18)
            subj = pair->value;
        l = l->next;
    }

    if (yd->yi->mail) {
    	if (who && email && subj) {
    	  char *from = g_strdup_printf("%s (%s)", who, email);
          yahoo_send_jabber_mail_notify(yd, -1, from, subj);
          g_free(from);
    	} else
          yahoo_send_jabber_mail_notify(yd, count, NULL, NULL);
    }
}

/* This is taken from Sylpheed by Hiroyuki Yamamoto.  We have our own tobase64 function
 * in util.c, but it has a bug I don't feel like finding right now ;) */
void to_y64(unsigned char *out, const unsigned char *in, int inlen) {
    char base64digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";
    /* raw bytes in quasi-big-endian order to base 64 string (NUL-terminated) */


    for (; inlen >= 3; inlen -= 3) {
        *out++ = base64digits[in[0] >> 2];
        *out++ = base64digits[((in[0] << 4) & 0x30) | (in[1] >> 4)];
        *out++ = base64digits[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
        *out++ = base64digits[in[2] & 0x3f];
        in += 3;
    }
    if (inlen > 0) {
        unsigned char fragment;

        *out++ = base64digits[in[0] >> 2];
        fragment = (in[0] << 4) & 0x30;
        if (inlen > 1)
            fragment |= in[1] >> 4;
        *out++ = base64digits[fragment];
        *out++ = (inlen < 2) ? '-' : base64digits[(in[1] << 2) & 0x3c];
        *out++ = '-';
    }
    *out = '\0';
}

#define SHA_CTX GAIM_SHA_CTX
#define shaInit(a) gaim_shaInit(a)
#define shaUpdate(a,b,c) gaim_shaUpdate((a),(b),(c))
#define shaFinal(a,b) gaim_shaFinal((a),(b))
static void yahoo_process_auth_new(struct yahoo_data *yd, const char *seed) {
    struct yahoo_packet *pack = NULL;
    const char *name = normalize(yd->username);
    const char *pass = yd->password;

	md5_byte_t			result[16];
	md5_state_t			ctx;

	SHA_CTX				ctx1;
	SHA_CTX				ctx2;

	char				*alphabet1			= "FBZDWAGHrJTLMNOPpRSKUVEXYChImkwQ";
	char				*alphabet2			= "F0E1D2C3B4A59687abcdefghijklmnop";

	char				*challenge_lookup	= "qzec2tb3um1olpar8whx4dfgijknsvy5";
	char				*operand_lookup		= "+|&%/*^-";
	char				*delimit_lookup		= ",;";

	char				*password_hash		= (char *)g_malloc(25);
	char				*crypt_hash		= (char *)g_malloc(25);
	char				*crypt_result		= NULL;

	char				pass_hash_xor1[64];
	char				pass_hash_xor2[64];
	char				crypt_hash_xor1[64];
	char				crypt_hash_xor2[64];
	char				resp_6[100];
	char				resp_96[100];

	unsigned char		digest1[20];
	unsigned char		digest2[20];
	unsigned char		comparison_src[20]; 
	unsigned char		magic_key_char[4];
	const unsigned char		*magic_ptr;

	unsigned int		magic[64];
	unsigned int		magic_work = 0;
	unsigned int		magic_4 = 0;

	int					x;
	int					y;
	int					cnt = 0;
	int					magic_cnt = 0;
	int					magic_len;

	memset(password_hash, 0, 25);
	memset(crypt_hash, 0, 25);
	memset(&pass_hash_xor1, 0, 64);
	memset(&pass_hash_xor2, 0, 64);
	memset(&crypt_hash_xor1, 0, 64);
	memset(&crypt_hash_xor2, 0, 64);
	memset(&digest1, 0, 20);
	memset(&digest2, 0, 20);
	memset(&magic, 0, 64);
	memset(&resp_6, 0, 100);
	memset(&resp_96, 0, 100);
	memset(&magic_key_char, 0, 4);
	memset(&comparison_src, 0, 20);

	/* 
	 * Magic: Phase 1.  Generate what seems to be a 30 byte value (could change if base64
	 * ends up differently?  I don't remember and I'm tired, so use a 64 byte buffer.
	 */

	magic_ptr = seed;

	while (*magic_ptr != (int)NULL) {
		char   *loc;
		
		/* Ignore parentheses.
		 */
		
		if (*magic_ptr == '(' || *magic_ptr == ')') {
			magic_ptr++;
			continue;
		}
		
		/* Characters and digits verify against the challenge lookup.
		 */
		
		if (isalpha(*magic_ptr) || isdigit(*magic_ptr)) {
			loc = strchr(challenge_lookup, *magic_ptr);
			if (!loc) {
			  /* SME XXX Error - disconnect here */
			}
			
			/* Get offset into lookup table and shl 3.
			 */
			
			magic_work = loc - challenge_lookup;
			magic_work <<= 3;
			
			magic_ptr++;
			continue;
		} else {
			unsigned int	local_store;
			
			loc = strchr(operand_lookup, *magic_ptr);
			if (!loc) {
				/* SME XXX Disconnect */
			}
			
			local_store = loc - operand_lookup;
			
			/* Oops; how did this happen?
			 */
			
			if (magic_cnt >= 64) 
				break;
			
			magic[magic_cnt++] = magic_work | local_store;
			magic_ptr++;
			continue;
		}
			}
	
	magic_len = magic_cnt;
	magic_cnt = 0;
	
	/* Magic: Phase 2.  Take generated magic value and sprinkle fairy dust on the values.
	 */

	for (magic_cnt = magic_len-2; magic_cnt >= 0; magic_cnt--) {
		unsigned char	byte1;
		unsigned char	byte2;
		
		/* Bad.  Abort.
		 */
		
		if ((magic_cnt + 1 > magic_len) || (magic_cnt > magic_len))
			break;
		
		byte1 = magic[magic_cnt];
		byte2 = magic[magic_cnt+1];
		
		byte1 *= 0xcd;
		byte1 ^= byte2;
		
		magic[magic_cnt+1] = byte1;
			}
	
	/* 
	 * Magic: Phase 3.  This computes 20 bytes.  The first 4 bytes are used as our magic
	 * key (and may be changed later); the next 16 bytes are an MD5 sum of the magic key
	 * plus 3 bytes.  The 3 bytes are found by looping, and they represent the offsets
	 * into particular functions we'll later call to potentially alter the magic key.
	 *
	 * %-)
	 */
	
	magic_cnt = 1;
	x = 0;
	
	do {
		unsigned int	bl = 0; 
		unsigned int	cl = magic[magic_cnt++];
		
		if (magic_cnt >= magic_len)
			break;
		
		if (cl > 0x7F) {
			if (cl < 0xe0) 
				bl = cl = (cl & 0x1f) << 6; 
			else {
				bl = magic[magic_cnt++]; 
				cl = (cl & 0x0f) << 6; 
				bl = ((bl & 0x3f) + cl) << 6; 
			} 

			cl = magic[magic_cnt++]; 
			bl = (cl & 0x3f) + bl; 
		} else
			bl = cl; 
		
		comparison_src[x++] = (bl & 0xff00) >> 8; 
		comparison_src[x++] = bl & 0xff; 
	} while (x < 20);
	
	/* First four bytes are magic key.
	 */
	
	memcpy(&magic_key_char[0], comparison_src, 4);
	magic_4 = magic_key_char[0] | (magic_key_char[1]<<8) | (magic_key_char[2]<<16) | (magic_key_char[3]<<24);
	
	/* 
	 * Magic: Phase 4.  Determine what function to use later by getting outside/inside
	 * loop values until we match our previous buffer.
	 */
	
	for (x = 0; x < 65535; x++) {
		int			leave = 0;

		for (y = 0; y < 5; y++) {
			md5_byte_t		result[16];
			md5_state_t		ctx;
			
			unsigned char	test[3];
			
			memset(&result, 0, 16);
			memset(&test, 0, 3);
			
			/* Calculate buffer.
			 */

			test[0] = x;
			test[1] = x >> 8;
			test[2] = y;
			
			md5_init(&ctx);
			md5_append(&ctx, magic_key_char, 4);
			md5_append(&ctx, test, 3);
			md5_finish(&ctx, result);
			
			if (!memcmp(result, comparison_src+4, 16)) {
				leave = 1;
				break;
			}
		}
		
		if (leave == 1)
			break;
	}
	
	/* If y != 0, we need some help.
	 */
	
	if (y != 0) {
		unsigned int	updated_key;
		
		/* Update magic stuff.   Call it twice because Yahoo's encryption is super bad ass.
		 */
		
		updated_key = yahoo_auth_finalCountdown(magic_4, 0x60, y, x);
		updated_key = yahoo_auth_finalCountdown(updated_key, 0x60, y, x);
		
		magic_key_char[0] = updated_key & 0xff;
		magic_key_char[1] = (updated_key >> 8) & 0xff;
		magic_key_char[2] = (updated_key >> 16) & 0xff;
		magic_key_char[3] = (updated_key >> 24) & 0xff;
	} 
	
/* Get password and crypt hashes as per usual.
	 */
	
	md5_init(&ctx);
	md5_append(&ctx, pass, strlen(pass));
	md5_finish(&ctx, result);
	to_y64(password_hash, result, 16);
	
	md5_init(&ctx);
	crypt_result = yahoo_crypt(pass, "$1$_2S43d5f$");  
	md5_append(&ctx, crypt_result, strlen(crypt_result));
	md5_finish(&ctx, result);
	to_y64(crypt_hash, result, 16);

	/* Our first authentication response is based off of the password hash.
	 */
	
	for (x = 0; x < (int)strlen(password_hash); x++) 
		pass_hash_xor1[cnt++] = password_hash[x] ^ 0x36;
	
	if (cnt < 64) 
		memset(&(pass_hash_xor1[cnt]), 0x36, 64-cnt);

	cnt = 0;
	
	for (x = 0; x < (int)strlen(password_hash); x++) 
		pass_hash_xor2[cnt++] = password_hash[x] ^ 0x5c;
	
	if (cnt < 64) 
		memset(&(pass_hash_xor2[cnt]), 0x5c, 64-cnt);
	
	shaInit(&ctx1);
	shaInit(&ctx2);
	
	/* 
	 * The first context gets the password hash XORed with 0x36 plus a magic value
	 * which we previously extrapolated from our challenge.
	 */
	
	shaUpdate(&ctx1, pass_hash_xor1, 64);
	if (y >= 3)
		ctx1.sizeLo = 0x1ff;
	shaUpdate(&ctx1, magic_key_char, 4);
	shaFinal(&ctx1, digest1);
	
	/* 
	 * The second context gets the password hash XORed with 0x5c plus the SHA-1 digest
	 * of the first context.
	 */
	
	shaUpdate(&ctx2, pass_hash_xor2, 64);
	shaUpdate(&ctx2, digest1, 20);
	shaFinal(&ctx2, digest2);
	
	/* 
	 * Now that we have digest2, use it to fetch characters from an alphabet to construct
	 * our first authentication response.
	 */

	for (x = 0; x < 20; x += 2) {
		unsigned int	val = 0;
		unsigned int	lookup = 0;
		
		char			byte[6];
				
		memset(&byte, 0, 6);

		/* First two bytes of digest stuffed together.
		 */

		val = digest2[x];
		val <<= 8;
		val += digest2[x+1];
		
		lookup = (val >> 0x0b);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet1))
			break;
		sprintf(byte, "%c", alphabet1[lookup]);
		strcat(resp_6, byte);
		strcat(resp_6, "=");

		lookup = (val >> 0x06);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_6, byte);
		
		lookup = (val >> 0x01);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_6, byte);

		lookup = (val & 0x01);
		if (lookup >= strlen(delimit_lookup))
			break;
		sprintf(byte, "%c", delimit_lookup[lookup]);
		strcat(resp_6, byte);
	}
	
	/* Our second authentication response is based off of the crypto hash.
	 */
	
	cnt = 0;
	memset(&digest1, 0, 20);
	memset(&digest2, 0, 20);
	
	for (x = 0; x < (int)strlen(crypt_hash); x++) 
		crypt_hash_xor1[cnt++] = crypt_hash[x] ^ 0x36;
	
	if (cnt < 64) 
		memset(&(crypt_hash_xor1[cnt]), 0x36, 64-cnt);

	cnt = 0;
	
	for (x = 0; x < (int)strlen(crypt_hash); x++) 
		crypt_hash_xor2[cnt++] = crypt_hash[x] ^ 0x5c;
	
	if (cnt < 64) 
		memset(&(crypt_hash_xor2[cnt]), 0x5c, 64-cnt);
	
	shaInit(&ctx1);
	shaInit(&ctx2);
	
	/* 
	 * The first context gets the password hash XORed with 0x36 plus a magic value
	 * which we previously extrapolated from our challenge.
	 */
	
	shaUpdate(&ctx1, crypt_hash_xor1, 64);
	if (y >= 3)
		ctx1.sizeLo = 0x1ff;
	shaUpdate(&ctx1, magic_key_char, 4);
	shaFinal(&ctx1, digest1);
	
	/* 
	 * The second context gets the password hash XORed with 0x5c plus the SHA-1 digest
	 * of the first context.
	 */
	
	shaUpdate(&ctx2, crypt_hash_xor2, 64);
	shaUpdate(&ctx2, digest1, 20);
	shaFinal(&ctx2, digest2);
	
	/* 
	 * Now that we have digest2, use it to fetch characters from an alphabet to construct
	 * our first authentication response.
	 */
	
	for (x = 0; x < 20; x += 2) {
		unsigned int	val = 0;
		unsigned int	lookup = 0;
		
		char			byte[6];
		
		memset(&byte, 0, 6);
		
		/* First two bytes of digest stuffed together.
		 */
		
		val = digest2[x];
		val <<= 8;
		val += digest2[x+1];

		lookup = (val >> 0x0b);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet1))
			break;
		sprintf(byte, "%c", alphabet1[lookup]);
		strcat(resp_96, byte);
		strcat(resp_96, "=");
		
		lookup = (val >> 0x06);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_96, byte);
		
		lookup = (val >> 0x01);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_96, byte);
		
		lookup = (val & 0x01);
		if (lookup >= strlen(delimit_lookup))
			break;
		sprintf(byte, "%c", delimit_lookup[lookup]);
		strcat(resp_96, byte);
	}
	
	pack = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP,	YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pack, 0, name);
	yahoo_packet_hash(pack, 6, resp_6);
	yahoo_packet_hash(pack, 96, resp_96);
	yahoo_packet_hash(pack, 1, name);
	yahoo_send_packet(yd, pack);
	yahoo_packet_free(pack);

	g_free(password_hash);
	g_free(crypt_hash);
}


static void yahoo_process_auth_old(struct yahoo_data *yd, const char *seed) {
    struct yahoo_packet *pack;
    const char *name = normalize(yd->username);
    const char *pass = yd->password;

    /* So, Yahoo has stopped supporting its older clients in India, and undoubtedly
     * will soon do so in the rest of the world.
     *
     * The new clients use this authentication method.  I warn you in advance, it's
     * bizzare, convoluted, inordinately complicated.  It's also no more secure than
     * crypt() was.  The only purpose this scheme could serve is to prevent third
     * part clients from connecting to their servers.
     *
     * Sorry, Yahoo.
     */

    md5_byte_t result[16];
    md5_state_t ctx;

    char *crypt_result;
    char password_hash[25];
    char crypt_hash[25];
    char *hash_string_p = g_malloc(50 + strlen(name));
    char *hash_string_c = g_malloc(50 + strlen(name));

    char checksum;

    int sv;

    char result6[25];
    char result96[25];

    sv = seed[15];
    sv = sv % 8;

    md5_init(&ctx);
    md5_append(&ctx, pass, strlen(pass));
    md5_finish(&ctx, result);
    to_y64(password_hash, result, 16);

    md5_init(&ctx);
    crypt_result = yahoo_crypt(pass, "$1$_2S43d5f$");
    md5_append(&ctx, crypt_result, strlen(crypt_result));
    md5_finish(&ctx, result);
    to_y64(crypt_hash, result, 16);

    switch (sv) {
    case 1:
    case 6:
        checksum = seed[seed[9] % 16];
        g_snprintf(hash_string_p, strlen(name) + 50,
                   "%c%s%s%s", checksum, name, seed, password_hash);
        g_snprintf(hash_string_c, strlen(name) + 50,
                   "%c%s%s%s", checksum, name, seed, crypt_hash);
        break;
    case 2:
    case 7:
        checksum = seed[seed[15] % 16];
        g_snprintf(hash_string_p, strlen(name) + 50,
                   "%c%s%s%s", checksum, seed, password_hash, name);
        g_snprintf(hash_string_c, strlen(name) + 50,
                   "%c%s%s%s", checksum, seed, crypt_hash, name);
        break;
    case 3:
        checksum = seed[seed[1] % 16];
        g_snprintf(hash_string_p, strlen(name) + 50,
                   "%c%s%s%s", checksum, name, password_hash, seed);
        g_snprintf(hash_string_c, strlen(name) + 50,
                   "%c%s%s%s", checksum, name, crypt_hash, seed);
        break;
    case 4:
        checksum = seed[seed[3] % 16];
        g_snprintf(hash_string_p, strlen(name) + 50,
                   "%c%s%s%s", checksum, password_hash, seed, name);
        g_snprintf(hash_string_c, strlen(name) + 50,
                   "%c%s%s%s", checksum, crypt_hash, seed, name);
        break;
    case 0:
    case 5:
        checksum = seed[seed[7] % 16];
        g_snprintf(hash_string_p, strlen(name) + 50,
                   "%c%s%s%s", checksum, password_hash, name, seed);
        g_snprintf(hash_string_c, strlen(name) + 50,
                   "%c%s%s%s", checksum, crypt_hash, name, seed);
        break;
    }

    md5_init(&ctx);
    md5_append(&ctx, hash_string_p, strlen(hash_string_p));
    md5_finish(&ctx, result);
    to_y64(result6, result, 16);

    md5_init(&ctx);
    md5_append(&ctx, hash_string_c, strlen(hash_string_c));
    md5_finish(&ctx, result);
    to_y64(result96, result, 16);

    pack = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP,	YAHOO_STATUS_AVAILABLE, 0);
    yahoo_packet_hash(pack, 0, name);
    yahoo_packet_hash(pack, 6, result6);
    yahoo_packet_hash(pack, 96, result96);
    yahoo_packet_hash(pack, 1, name);

    yahoo_send_packet(yd, pack);

    g_free(hash_string_p);
    g_free(hash_string_c);

    yahoo_packet_free(pack);

}

static void yahoo_process_auth(struct yahoo_data *yd, struct yahoo_packet *pkt) {
    char *seed = NULL;
    char *sn   = NULL;
    GSList *l = pkt->hash;
    int m = 1;


    while (l) {
        struct yahoo_pair *pair = l->data;
        if (pair->key == 94)
            seed = pair->value;
        if (pair->key == 1)
            sn = pair->value;
        if (pair->key == 13)
            m = atoi(pair->value);
        l = l->next;
    }

    if (seed) {
        switch (m) {
        case 0:
            yahoo_process_auth_old(yd, seed);
            break;
        case 1:
            yahoo_process_auth_new(yd, seed);
            break;
        default:
            yahoo_process_auth_new(yd, seed); /* Can't hurt to try it anyway. */
        }
    }
}

static void yahoo_packet_process(struct yahoo_data *yd, struct yahoo_packet *pkt) {
    switch (pkt->service) {
    case YAHOO_SERVICE_LOGON:
    case YAHOO_SERVICE_LOGOFF:
    case YAHOO_SERVICE_ISAWAY:
    case YAHOO_SERVICE_ISBACK:
    case YAHOO_SERVICE_GAMELOGON:
    case YAHOO_SERVICE_GAMELOGOFF:
        log_debug(ZONE, "[YAHOO]: Process Status");
        yahoo_process_status(yd, pkt);
        break;
    case YAHOO_SERVICE_NOTIFY:
        log_debug(ZONE, "[YAHOO]: Process Service Notify");
        yahoo_process_notify(yd, pkt);
        break;
    case YAHOO_SERVICE_MESSAGE:
    case YAHOO_SERVICE_GAMEMSG:
        log_debug(ZONE, "[YAHOO]: Process Message");
        yahoo_process_message(yd, pkt);
        break;
    case YAHOO_SERVICE_NEWMAIL:
        log_debug(ZONE, "[YAHOO]: Process New Mail");
        yahoo_process_mail(yd, pkt);
        break;
    case YAHOO_SERVICE_NEWCONTACT:
        log_debug(ZONE, "[YAHOO]: Process New Contact");
        yahoo_process_contact(yd, pkt);
        break;
    case YAHOO_SERVICE_LIST:
        log_debug(ZONE, "[YAHOO]: Process Service List");
        yahoo_process_list(yd, pkt);
        break;
    case YAHOO_SERVICE_AUTH:
        log_debug(ZONE, "[YAHOO]: Process Auth");
        yahoo_process_auth(yd, pkt);
        break;
    default:
        log_debug(ZONE, "unhandled service 0x%02x\n", pkt->service);
        break;
    }
}

void yahoo_pending(mio m, int flag, void *arg, char *buf, int len) {
    struct yahoo_data *yd = (struct yahoo_data *)arg;

    if (flag == MIO_CLOSED) {
        log_debug(ZONE, "[YAHOO]: MIO_CLOSE (fd=%d)", m->fd);
        if (yd)
            yahoo_remove_session_yd(yd);
        return;
    }

    if (flag == MIO_ERROR) {
        xmlnode x;
        while((x = mio_cleanup(m)) != NULL)
            deliver_fail(dpacket_new(x), "Socket Error to Yahoo! Server");
        log_debug(ZONE, "[YAHOO]: ERROR from Yahoo! server.");
        if (yd)
            yahoo_remove_session_yd(yd);
        return;
    }

    if(flag == MIO_NEW) {
        log_debug(ZONE, "[YAHOO]: '%s' connected to Yahoo! server. [%s]", jid_full(yd->me),yd->username);
        yd->fd = m;
        yahoo_got_connected(yd);
        return;
    }

    if (len <= 0) {
        // FUNC		hide_login_progress_error(gc, "Unable to read");
        // FUNC		signoff(gc);
        return;
    }
    /*
       From here on, this *should* be a read from the Yahoo server. In this case, we want to 
       send this off to its own thread for processing.
    */
    yd->buf = buf;
    yd->len = len;
#ifdef _JCOMP
    yahoo_read_data((void *)yd);
#else
    pth_spawn(NULL, yahoo_read_data, (void *)yd);
#endif
}

void yahoo_read_data(void *td) {
    struct yahoo_data *yd = (struct yahoo_data *)td;

    log_debug(ZONE, "[YAHOO]: Read %d [%d] bytes (fd=%d)  for '%s'", yd->len, yd->rxlen, yd->fd->fd, jid_full(yd->me));
    yd->yi->stats->bytes_in += yd->len;
    yd->rxqueue = g_realloc(yd->rxqueue, yd->len + yd->rxlen);
    memcpy(yd->rxqueue + yd->rxlen, yd->buf, yd->len);
    yd->rxlen += yd->len;

    while (1) {
        struct yahoo_packet *pkt;
        int pos = 0;
        int pktlen;

        if (yd->rxlen < YAHOO_PACKET_HDRLEN)
            return;

        pos += 4; /* YMSG */
        pos += 2;
        pos += 2;

        pktlen = yahoo_get16(yd->rxqueue + pos); pos += 2;
        log_debug(ZONE, "[YAHOO]: %d bytes to read, rxlen is %d\n", pktlen, yd->rxlen);

        if (yd->rxlen < (YAHOO_PACKET_HDRLEN + pktlen))
            return;

        yahoo_packet_dump(yd->rxqueue, YAHOO_PACKET_HDRLEN + pktlen);

        pkt = yahoo_packet_new(0, 0, 0);

        pkt->service = yahoo_get16(yd->rxqueue + pos); pos += 2;
        pkt->status = yahoo_get32(yd->rxqueue + pos); pos += 4;
        log_debug(ZONE, "[YAHOO]: Service: 0x%02x Status: %d\n", pkt->service, pkt->status);
        pkt->id = yahoo_get32(yd->rxqueue + pos); pos += 4;

        yahoo_packet_read(pkt, yd->rxqueue + pos, pktlen);

        yd->rxlen -= YAHOO_PACKET_HDRLEN + pktlen;
        if (yd->rxlen) {
            char *tmp = g_memdup(yd->rxqueue + YAHOO_PACKET_HDRLEN + pktlen, yd->rxlen);
            g_free(yd->rxqueue);
            yd->rxqueue = tmp;
        } else {
            g_free(yd->rxqueue);
            yd->rxqueue = NULL;
        }

        yahoo_packet_process(yd, pkt);

        yahoo_packet_free(pkt);
    }
}

void yahoo_got_connected(struct yahoo_data *yd) {
    struct yahoo_packet *pkt;

    //	if (!g_slist_find(connections, gc)) {
    //		close(source);
    //		return;
    //  	}
    //
    //  	if (source < 0) {
    //  		hide_login_progress(gc, "Unable to connect");
    //  		signoff(gc);
    //  		return;
    //  	}
    //
    //  	yd = gc->proto_data;
    //  	yd->fd = source;
    //
    pkt = yahoo_packet_new(YAHOO_SERVICE_AUTH, YAHOO_STATUS_AVAILABLE, 0);

    yahoo_packet_hash(pkt, 1, normalize(yd->username));
    yahoo_send_packet(yd, pkt);

    yahoo_packet_free(pkt);
    //
    //  	gc->inpa = gaim_input_add(yd->fd, GAIM_INPUT_READ, yahoo_pending, gc);
}

//  static void yahoo_login(struct aim_user *user) {
//  	jpacket jp = new_gaim_conn(user);
//  	struct yahoo_data *yd = (struct yahoo_data *)jp->aux1;
//
//  	set_login_progress(gc, 1, "Connecting");
//
//  	yd->fd = -1;
//  	yd->hash = g_hash_table_new(g_str_hash, g_str_equal);
//  	yd->games = g_hash_table_new(g_str_hash, g_str_equal);
//
//
//  	if (!g_strncasecmp(user->proto_opt[USEROPT_PAGERHOST], "cs.yahoo.com", strlen("cs.yahoo.com"))) {
//  		/* Figured out the new auth method -- cs.yahoo.com likes to disconnect on buddy remove and add now */
//  		debug_printf("Setting new Yahoo! server.\n");
//  		g_snprintf(user->proto_opt[USEROPT_PAGERHOST], strlen("scs.yahoo.com") + 1, "scs.yahoo.com");
//  		save_prefs();
//  	}
//
//
//         	if (proxy_connect(user->proto_opt[USEROPT_PAGERHOST][0] ?
//  				user->proto_opt[USEROPT_PAGERHOST] : YAHOO_PAGER_HOST,
//  			   user->proto_opt[USEROPT_PAGERPORT][0] ?
//  				atoi(user->proto_opt[USEROPT_PAGERPORT]) : YAHOO_PAGER_PORT,
//  			   yahoo_got_connected, gc) < 0) {
//  		hide_login_progress(gc, "Connection problem");
//  		signoff(gc);
//  		return;
//  	}
//
//  }
/*
static gboolean yahoo_destroy_hash(gpointer key, gpointer val, gpointer data) {
    g_free(key);
    g_free(val);
    return TRUE;
}
*/
void yahoo_close(struct yahoo_data *yd) {

    // g_hash_table_foreach_remove(yd->hash, yahoo_destroy_hash, NULL);
    // g_hash_table_destroy(yd->hash);
    // g_hash_table_foreach_remove(yd->games, yahoo_destroy_hash, NULL);
    // g_hash_table_destroy(yd->games);
    log_debug(ZONE, "[YAHOO] yahoo_close() called");
    if (yd->fd)
        mio_close(yd->fd);
    if (yd->rxqueue)
        g_free(yd->rxqueue);
    yd->rxlen = 0;
    yahoo_remove_session_yd(yd);
    //	if (gc->inpa)
    //		gaim_input_remove(gc->inpa);
    //	g_free(yd);
}

//  static char **yahoo_list_icon(int uc)
//  {
//  	if ((uc >> 2) == YAHOO_STATUS_IDLE)
//  		return status_idle_xpm;
//  	else if (uc & UC_UNAVAILABLE)
//  		return status_away_xpm;
//  	else if (uc & YAHOO_STATUS_GAME)
//  		return status_game_xpm;
//  	return status_here_xpm;
//  }

char *yahoo_get_status_string(enum yahoo_status a) {
    switch (a) {
    case YAHOO_STATUS_BRB:
        return "Be Right Back";
    case YAHOO_STATUS_BUSY:
        return "Busy";
    case YAHOO_STATUS_NOTATHOME:
        return "Not At Home";
    case YAHOO_STATUS_NOTATDESK:
        return "Not At Desk";
    case YAHOO_STATUS_NOTINOFFICE:
        return "Not In Office";
    case YAHOO_STATUS_ONPHONE:
        return "On Phone";
    case YAHOO_STATUS_ONVACATION:
        return "On Vacation";
    case YAHOO_STATUS_OUTTOLUNCH:
        return "Out To Lunch";
    case YAHOO_STATUS_STEPPEDOUT:
        return "Stepped Out";
    case YAHOO_STATUS_INVISIBLE:
        return "Invisible";
    default:
        return "Online";
    }
}

//  static void yahoo_game(struct yahoo_data *yd, char *name) {
//     char *game = g_hash_table_lookup(yd->games, name);
//     char *t;
//     char url[256];

//     if (!game)
//        return;
//     t = game = g_strdup(strstr(game, "ante?room="));
//     while (*t != '\t')
//        t++;
//     *t = 0;
//     g_snprintf(url, sizeof url, "http://games.yahoo.com/games/%s", game);
//     open_url(NULL, url);
//     g_free(game);
//  }
//  static GList *yahoo_buddy_menu(jpacket jp, char *who)
//  {
//  	GList *m = NULL;
//  	struct proto_buddy_menu *pbm;
//  	struct yahoo_data *yd = (struct yahoo_data *)jp->aux1;
//  	struct buddy *b = find_buddy(gc, who); /* this should never be null. if it is,
//  						  segfault and get the bug report. */
//  	static char buf[1024];
//  	static char buf2[1024];
//
//  	if (b->uc & UC_UNAVAILABLE && b->uc >> 2 != YAHOO_STATUS_IDLE) {
//  		pbm = g_new0(struct proto_buddy_menu, 1);
//  		if ((b->uc >> 2) != YAHOO_STATUS_CUSTOM)
//  			g_snprintf(buf, sizeof buf,
//  				   "Status: %s", yahoo_get_status_string(b->uc >> 2));
//  		else
//  			g_snprintf(buf, sizeof buf, "Custom Status: %s",
//  				   (char *)g_hash_table_lookup(yd->hash, b->name));
//  		pbm->label = buf;
//  		pbm->callback = NULL;
//  		pbm->gc = gc;
//  		m = g_list_append(m, pbm);
//  	}
//
//  	if (b->uc | YAHOO_STATUS_GAME) {
//  		char *game = g_hash_table_lookup(yd->games, b->name);
//  		char *room;
//  		if (!game)
//  			return m;
//  		if (game) {
//  			char *t;
//  			pbm = g_new0(struct proto_buddy_menu, 1);
//  			if (!(room = strstr(game, "&follow="))) /* skip ahead to the url */
//  				return NULL;
//  			while (*room && *room != '\t')          /* skip to the tab */
//  				room++;
//  			t = room++;                             /* room as now at the name */
//  			while (*t != '\n')
//  				t++;                            /* replace the \n with a space */
//  			*t = ' ';
//  			g_snprintf(buf2, sizeof buf2, "%s", room);
//  			pbm->label = buf2;
//  			pbm->callback = yahoo_game;
//  			pbm->gc = gc;
//  			m = g_list_append(m, pbm);
//  		}
//  	}
//
//  	return m;
//  }

//  static GList *yahoo_user_opts()
//  {
//  	GList *m = NULL;
//  	struct proto_user_opt *puo;
//
//  	puo = g_new0(struct proto_user_opt, 1);
//  	puo->label = "Pager Host:";
//  	puo->def = YAHOO_PAGER_HOST;
//  	puo->pos = USEROPT_PAGERHOST;
//  	m = g_list_append(m, puo);
//
//  	puo = g_new0(struct proto_user_opt, 1);
//  	puo->label = "Pager Port:";
//  	puo->def = "5050";
//  	puo->pos = USEROPT_PAGERPORT;
//  	m = g_list_append(m, puo);
//
//  	return m;
//  }
//
void yahoo_act_id(struct yahoo_data *yd, char *entry) {
    struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_IDACT, YAHOO_STATUS_AVAILABLE, 0);
    yahoo_packet_hash(pkt, 3, entry);
    yahoo_send_packet(yd, pkt);
    yahoo_packet_free(pkt);

    g_snprintf(yd->displayname, sizeof(yd->displayname), "%s", entry);
}
//
//  static void yahoo_do_action(jpacket jp, char *act)
//  {
//  	if (!strcmp(act, "Activate ID")) {
//  		do_prompt_dialog("Activate which ID:", gc->displayname, gc, yahoo_act_id, NULL);
//  	}
//  }
//
//  static GList *yahoo_actions() {
//  	GList *m = NULL;
//
//  	m = g_list_append(m, "Activate ID");
//
//  	return m;
//  }

int yahoo_send_im(struct yahoo_data *yd, char *who, char *what, int len, int flags) {
    struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_MESSAGE, YAHOO_STATUS_AVAILABLE, 0);

    yahoo_packet_hash(pkt, 1, yd->displayname);
    yahoo_packet_hash(pkt, 5, who);
    yahoo_packet_hash(pkt, 14, what);
    yahoo_packet_hash(pkt, 97, "1");
    yahoo_send_packet(yd, pkt);

    yahoo_packet_free(pkt);

    return 1;
}

int yahoo_send_typing(struct yahoo_data *yd, char *who, int typ) {
    struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_NOTIFY, YAHOO_STATUS_TYPING, 0);
    yahoo_packet_hash(pkt, 49, "TYPING");
    yahoo_packet_hash(pkt, 1, yd->displayname);
    yahoo_packet_hash(pkt, 14, " ");
    yahoo_packet_hash(pkt, 13, typ ? "1" : "0");
    yahoo_packet_hash(pkt, 5, who);
    yahoo_packet_hash(pkt, 1002, "1");

    yahoo_send_packet(yd, pkt);

    yahoo_packet_free(pkt);

    return 0;
}

void yahoo_set_away(struct yahoo_data *yd, int state, char *msg) {
    struct yahoo_packet *pkt;
    int service;
    char s[4];

    if (state == 0) {
        service = YAHOO_SERVICE_ISBACK;
        yd->current_status = YAHOO_STATUS_AVAILABLE;
    } else {
        service = YAHOO_SERVICE_ISAWAY;
        yd->current_status = YAHOO_STATUS_CUSTOM;
    }
    if (msg == NULL)
        msg = " ";

    pkt = yahoo_packet_new(service, yd->current_status, 0);
    if (state  == 0) {
        g_snprintf(s, sizeof(s), "%d", YAHOO_STATUS_AVAILABLE);
        yahoo_packet_hash(pkt, 10, s);
    } else {
        g_snprintf(s, sizeof(s), "%d", YAHOO_STATUS_CUSTOM);
        yahoo_packet_hash(pkt, 10, s);
        g_snprintf(s, sizeof(s), "%d", 0);
        yahoo_packet_hash(pkt, 47, "1");
        yahoo_packet_hash(pkt, 19, msg);
    }

//  log_notice(ZONE, "[YAHOO]: Presence service=0x%04x status=0x%04x", service,yd->current_status);
    yahoo_send_packet(yd, pkt);
    yahoo_packet_free(pkt);
}
/*
static void yahoo_set_idle(struct yahoo_data *yd, int idle) {
    struct yahoo_packet *pkt = NULL;

    if (idle && yd->current_status == YAHOO_STATUS_AVAILABLE) {
        pkt = yahoo_packet_new(YAHOO_SERVICE_ISAWAY, YAHOO_STATUS_IDLE, 0);
        yd->current_status = YAHOO_STATUS_IDLE;
    } else if (!idle && yd->current_status == YAHOO_STATUS_IDLE) {
        pkt = yahoo_packet_new(YAHOO_SERVICE_ISAWAY, YAHOO_STATUS_AVAILABLE, 0);
        yd->current_status = YAHOO_STATUS_AVAILABLE;
    }

    if (pkt) {
        char buf[4];
        g_snprintf(buf, sizeof(buf), "%d", yd->current_status);
        yahoo_packet_hash(pkt, 10, buf);
        yahoo_send_packet(yd, pkt);
        yahoo_packet_free(pkt);
    }
}

static GList *yahoo_away_states(struct yahoo_data *yd) {
    GList *m = NULL;

    m = g_list_append(m, "Available");
    m = g_list_append(m, "Be Right Back");
    m = g_list_append(m, "Busy");
    m = g_list_append(m, "Not At Home");
    m = g_list_append(m, "Not At Desk");
    m = g_list_append(m, "Not In Office");
    m = g_list_append(m, "On Phone");
    m = g_list_append(m, "On Vacation");
    m = g_list_append(m, "Out To Lunch");
    m = g_list_append(m, "Stepped Out");
    m = g_list_append(m, "Invisible");
    //	m = g_list_append(m, GAIM_AWAY_CUSTOM);

    return m;
}

static void yahoo_keepalive(struct yahoo_data *yd) {
    struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_PING, YAHOO_STATUS_AVAILABLE, 0);
    yahoo_send_packet(yd, pkt);
    yahoo_packet_free(pkt);
}
*/
void yahoo_add_buddy(struct yahoo_data *yd, char *who, char *group) {
    struct yahoo_packet *pkt;
    //	struct group *g;
    //	char *group = NULL;
    //
    if (!yd->logged_in)
        return;

    //  	g = find_group_by_buddy(gc, who);
    //  	if (g)
    //  		group = g->name;
    //  	else
    //  		group = "Buddies";

    pkt = yahoo_packet_new(YAHOO_SERVICE_ADDBUDDY, YAHOO_STATUS_AVAILABLE, 0);
    yahoo_packet_hash(pkt, 1, yd->displayname);
    yahoo_packet_hash(pkt, 7, who);
    yahoo_packet_hash(pkt, 65, group);
    yahoo_send_packet(yd, pkt);
    yahoo_packet_free(pkt);
}

void yahoo_remove_buddy(struct yahoo_data *yd, char *who, char *group) {

    struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, 0);
    yahoo_packet_hash(pkt, 1, yd->displayname);
    yahoo_packet_hash(pkt, 7, who);
    yahoo_packet_hash(pkt, 65, group);
    yahoo_send_packet(yd, pkt);
    yahoo_packet_free(pkt);
}
//
//
//  GSList *yahoo_smiley_list()
//  {
//  	GSList *smilies = NULL;
//
//  	smilies = add_smiley(smilies, "=:)", yahoo_alien, 1);
//  	smilies = add_smiley(smilies, "=:-)", yahoo_alien, 0);
//  	smilies = add_smiley(smilies, "o:)", yahoo_angel, 0);
//  	smilies = add_smiley(smilies, "o:-)", yahoo_angel, 0);
//  	smilies = add_smiley(smilies, "0:)", yahoo_angel, 0);
//  	smilies = add_smiley(smilies, "0:-)", yahoo_angel, 0);
//  	smilies = add_smiley(smilies, "X-(", yahoo_angry, 1);
//  	smilies = add_smiley(smilies, "X(", yahoo_angry, 0);
//  	smilies = add_smiley(smilies, "x-(", yahoo_angry, 0);
//  	smilies = add_smiley(smilies, "x(", yahoo_angry, 0);
//  	smilies = add_smiley(smilies, ":D", yahoo_bigsmile, 1);
//  	smilies = add_smiley(smilies, ":-D", yahoo_bigsmile, 0);
//  	smilies = add_smiley(smilies, ":\">", yahoo_blush, 1);
//  	smilies = add_smiley(smilies, "=;", yahoo_bye, 1);
//  	smilies = add_smiley(smilies, ":o)", yahoo_clown, 1);
//  	smilies = add_smiley(smilies, ":0)", yahoo_clown, 0);
//  	smilies = add_smiley(smilies, ":O)", yahoo_clown, 0);
//  	smilies = add_smiley(smilies, "<@:)", yahoo_clown, 0);
//  	smilies = add_smiley(smilies, "3:-0", yahoo_cow, 1);
//  	smilies = add_smiley(smilies, "3:-o", yahoo_cow, 0);
//  	smilies = add_smiley(smilies, "3:-O", yahoo_cow, 0);
//  	smilies = add_smiley(smilies, "3:O", yahoo_cow, 0);
//  	smilies = add_smiley(smilies, "<):)", yahoo_cowboy, 1);
//  	smilies = add_smiley(smilies, ":((", yahoo_cry, 1);
//  	smilies = add_smiley(smilies, ":-((", yahoo_cry, 0);
//  	smilies = add_smiley(smilies, ">:)", yahoo_devil, 1);
//  	smilies = add_smiley(smilies, "@};-", yahoo_flower, 1);
//  	smilies = add_smiley(smilies, "8-X", yahoo_ghost, 1);
//  	smilies = add_smiley(smilies, ":B", yahoo_glasses, 1);
//  	smilies = add_smiley(smilies, ":-B", yahoo_glasses, 0);
//  	smilies = add_smiley(smilies, ":))", yahoo_laughloud, 1);
//  	smilies = add_smiley(smilies, ":-))", yahoo_laughloud, 0);
//  	smilies = add_smiley(smilies, ":x", yahoo_love, 1);
//  	smilies = add_smiley(smilies, ":-x", yahoo_love, 0);
//  	smilies = add_smiley(smilies, ":X", yahoo_love, 0);
//  	smilies = add_smiley(smilies, ":-X", yahoo_love, 0);
//  	smilies = add_smiley(smilies, ":>", yahoo_mean, 1);
//  	smilies = add_smiley(smilies, ":->", yahoo_mean, 0);
//  	smilies = add_smiley(smilies, ":|", yahoo_neutral, 1);
//  	smilies = add_smiley(smilies, ":-|", yahoo_neutral, 0);
//  	smilies = add_smiley(smilies, ":O", yahoo_ooooh, 1);
//  	smilies = add_smiley(smilies, ":-O", yahoo_ooooh, 0);
//  	smilies = add_smiley(smilies, ":-\\", yahoo_question, 1);
//  	smilies = add_smiley(smilies, ":-/", yahoo_question, 0);
//  	smilies = add_smiley(smilies, ":(", yahoo_sad, 1);
//  	smilies = add_smiley(smilies, ":-(", yahoo_sad, 0);
//  	smilies = add_smiley(smilies, "I-)", yahoo_sleep, 1);
//  	smilies = add_smiley(smilies, "|-)", yahoo_sleep, 0);
//  	smilies = add_smiley(smilies, "I-|", yahoo_sleep, 0);
//  	smilies = add_smiley(smilies, ":)", yahoo_smiley, 1);
//  	smilies = add_smiley(smilies, ":-)", yahoo_smiley, 0);
//  	smilies = add_smiley(smilies, "(:", yahoo_smiley, 0);
//  	smilies = add_smiley(smilies, "(-:", yahoo_smiley, 0);
//  	smilies = add_smiley(smilies, "B-)", yahoo_sunglas, 1);
//  	smilies = add_smiley(smilies, ":-p", yahoo_tongue, 1);
//  	smilies = add_smiley(smilies, ":p", yahoo_tongue, 0);
//  	smilies = add_smiley(smilies, ":P", yahoo_tongue, 0);
//  	smilies = add_smiley(smilies, ":-P", yahoo_tongue, 0);
//  	smilies = add_smiley(smilies, ";)", yahoo_wink, 1);
//  	smilies = add_smiley(smilies, ";-)", yahoo_wink, 0);
//
//
//  	return smilies;
//  }
//
//  static struct prpl *my_protocol = NULL;
//
//  void yahoo_init(struct prpl *ret) {
//  	ret->protocol = PROTO_YAHOO;
//  	ret->options = OPT_PROTO_MAIL_CHECK;
//  	ret->name = yahoo_name;
//  	ret->user_opts = yahoo_user_opts;
//  	ret->login = yahoo_login;
//  	ret->close = yahoo_close;
//  	ret->buddy_menu = yahoo_buddy_menu;
//  	ret->list_icon = yahoo_list_icon;
//  	ret->actions = yahoo_actions;
//  	ret->do_action = yahoo_do_action;
//  	ret->send_im = yahoo_send_im;
//  	ret->away_states = yahoo_away_states;
//  	ret->set_away = yahoo_set_away;
//  	ret->set_idle = yahoo_set_idle;
//  	ret->keepalive = yahoo_keepalive;
//  	ret->add_buddy = yahoo_add_buddy;
//  	ret->remove_buddy = yahoo_remove_buddy;
//  	ret->send_typing = yahoo_send_typing;
//  	ret->smiley_list = yahoo_smiley_list;
//
//  	my_protocol = ret;
//  }
//
//  #ifndef STATIC
//
//  char *gaim_plugin_init(GModule *handle)
//  {
//  	load_protocol(yahoo_init, sizeof(struct prpl));
//  	return NULL;
//  }
//
//  void gaim_plugin_remove()
//  {
//  	struct prpl *p = find_prpl(PROTO_YAHOO);
//  	if (p == my_protocol)
//  		unload_protocol(p);
//  }
//
//  char *name()
//  {
//  	return "Yahoo";
//  }
//
//  char *description()
//  {
//  	return PRPL_DESC("Yahoo");
//  }
//
//  #endif
//
