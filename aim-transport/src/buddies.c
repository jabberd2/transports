/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"

struct buddy_clean_data
{
    ati ti;
    xmlnode node;
};

void _at_buddies_unsubscribe(xht ht, const char *key, void *data, void * arg)
{
    xmlnode pres;
    at_session s;
    ati ti;

    s = (at_session)arg;
    ti = s->ti;

    pres = jutil_presnew(JPACKET__UNSUBSCRIBE, jid_full(s->cur), "Transport Removal");
    xmlnode_put_attrib(pres, "from", key);
    at_deliver(ti,pres);
}

result at_buddy_pending_clean(void *arg)
{
    struct buddy_clean_data *bcd;
    ati ti;
    char *user;
    xmlnode node;

    bcd = (struct buddy_clean_data *)arg;
    node = bcd->node;
    ti = bcd->ti;
    user = xmlnode_get_attrib(node, "user");

    log_debug(ZONE, "[AT] Cleaning pending for %s: %s", user, xmlnode2str(node));
    pth_mutex_acquire(&ti->buddies_mutex, 0, NULL);

    xhash_zap(ti->pending__buddies, user);

    xmlnode_free(node);
    pth_mutex_release(&ti->buddies_mutex);

    return r_UNREG;
}

void at_buddy_addtolist(at_session s, spool sp, xmlnode x)
{
    at_buddy new;
    at_buddy old;
    xmlnode item;

    for(item = xmlnode_get_firstchild(x); item != NULL; 
                    item = xmlnode_get_nextsibling(item))
    {
        char *sn;

        sn = at_normalize(xmlnode_get_attrib(item, "name"));
        old = xhash_get(s->buddies, sn);
        if(old != NULL)
        {
            log_debug(ZONE, "[AT] We already have %s in our list", sn);
            continue;
        }

        log_debug(ZONE, "[AIM] Adding buddy %s", sn);

        spooler(sp, sn, "&", sp);
        new = pmalloco(s->p, sizeof(_at_buddy));
        new->full = jid_new(s->p, s->ti->i->id);
        jid_set(new->full, sn, JID_USER);
        new->last = xmlnode_new_tag_pool(s->p,"query");
        new->is_away = -1;
        xmlnode_put_attrib(new->last,"xmlns",NS_LAST);
        xmlnode_put_attrib(new->last,"stamp",jutil_timestamp()); 

        xhash_put(s->buddies, new->full->user, new);
    }
}

char *at_buddy_buildlist(at_session s, jid from)
{
    char *list;
	char *blist;
	spool sp;
    pool p, bp;
    xmlnode x;
    xmlnode msg;

    p = pool_new();
	sp = spool_new(p);

    log_debug(ZONE, "[AIM] Building buddy list for new session - XDB");

    /* Get buddies from transport's XDB roster */
    x = at_xdb_get(s->ti, from, AT_NS_ROSTER);
    if(x != NULL)
    {
        at_buddy_addtolist(s, sp, x);
    }

    log_debug(ZONE, "[AIM] Building buddy list for new session - pending list");

    /* Get buddies from instance's pending list - this list */
    /* has been built by incoming presence/probe packets before */
    /* a session has been established */
    x = xhash_get(s->ti->pending__buddies, jid_full(jid_user(from)));
    if(x == NULL)
    {
        return NULL;
    } else {
        at_buddy_addtolist(s, sp, x);
    }

    list = spool_print(sp);
    if(list)
    {
        blist = strdup(list);
    } else {
        blist = NULL;
    }
    log_debug(ZONE, "[AT] Buddylist generation complete");

    pool_free(p);

    return blist;
}

int at_buddy_subscribe(ati ti, jpacket jp)
{
    xmlnode dup,dup2;
    at_session s=at_session_find_by_jid(ti, jp->from);

    /* check for a valid session */
    if(s==NULL)
    {
        xmlnode err,error;
        /* if no session, and subscribing to a buddy, not aim-t */
        err=xmlnode_new_tag("message");
        xmlnode_put_attrib(err,"type","error");
        xmlnode_put_attrib(err,"from",ti->i->id);
        xmlnode_put_attrib(err,"to",jid_full(jp->from));
        error=xmlnode_insert_tag(err,"error");
        xmlnode_insert_cdata(error,"Cannot Subscribe to a AIM Buddy without a registration",-1);
        xmlnode_put_attrib(error,"code","407");
        at_deliver(ti,err);

        return 0;
    }
    /* XXX
	 * Create the duplicate for the subscribe notice
	 * Construct a subscribed notification
	 * 
	 * I had to do these in this odd order due to memory concerns
	 * Dunno why I didn't just make two dups.
	 *
	 */
	dup = xmlnode_dup(jp->x);
	dup2 = xmlnode_dup(jp->x);

	xmlnode_put_attrib(dup, "to", jid_full(jp->from));
	xmlnode_put_attrib(dup, "from", jid_full(jp->to));
	xmlnode_put_attrib(dup, "type", "subscribed");
	log_debug(ZONE, "[AIM] Sending subscribed notice\n");
    at_deliver(ti,dup);

	/* Construct the subscribe back */
	xmlnode_put_attrib(dup2, "type", "subscribe");
    xmlnode_put_attrib(dup2, "to", jid_full(jp->from));
    xmlnode_put_attrib(dup2, "from", jid_full(jp->to));
	log_debug(ZONE, "[AIM] Asking for a subscribe\n");
    at_deliver(ti,dup2);

	return 0;
}

/** Got some hint for a buddy (presence/probe/s10n Jabber packet) */
int at_buddy_add(ati ti, jpacket jp)
{
	at_session s;
	at_buddy buddy;
    pool p;
	int newbud=0;
    struct buddy_clean_data *bcd;
	
	s = at_session_find_by_jid(ti, jp->from);
	if(s==NULL || s->loggedin == 0) 
    {
        xmlnode cur, item;

        /* No session yet, add to pending list for this user */
        log_debug(ZONE, "[AIM] Add buddy %s to pending list for %s", jid_full(jp->to),jid_full(jid_user(jp->from)));
        pth_mutex_acquire(&ti->buddies_mutex, 0, NULL);
        cur = xhash_get(ti->pending__buddies, jid_full(jid_user(jp->from)));
        if(cur == NULL)
        {
            log_debug(ZONE, "[AIM] Creating pending list for %s", jid_full(jid_user(jp->from)));
            cur = xmlnode_new_tag("buddies");
            xmlnode_put_attrib(cur, "user", jid_full(jid_user(jp->from)));
            bcd = pmalloco(xmlnode_pool(cur), sizeof(struct buddy_clean_data));
            bcd->node = cur;
            bcd->ti = ti;
            /* Free pending list after a while */
            register_beat(30, at_buddy_pending_clean, bcd);
        }
        item = xmlnode_insert_tag(cur, "item");
        xmlnode_put_attrib(item, "name", jp->to->user);
        xmlnode_free(jp->x);
        log_debug(ZONE, "[AT] Resulting pending list: %s", xmlnode2str(cur));
        xhash_put(ti->pending__buddies, xmlnode_get_attrib(cur, "user"), cur);
        pth_mutex_release(&ti->buddies_mutex);
        return 1;
	}

    /* We have a session */
    
    if(xhash_get(s->buddies,at_normalize(jp->to->user)) == NULL)
    {
        log_debug(ZONE, "[AIM] Add buddy %s to session %s\n", jp->to->user, jid_full(jp->from));
        buddy = pmalloco(s->p, sizeof(_at_buddy));
        buddy->full=jid_new(s->p, jid_full(jp->to));
        buddy->last = xmlnode_new_tag_pool(s->p,"query");
        buddy->is_away = -1;
        xmlnode_put_attrib(buddy->last,"xmlns",NS_LAST);
        xmlnode_put_attrib(buddy->last,"stamp",jutil_timestamp());
        xhash_put(s->buddies, buddy->full->user, buddy);
        at_buddy_subscribe(ti, jp);
	aim_add_buddy(s->ass, aim_getconn_type(s->ass, AIM_CONN_TYPE_BOS),
                  jp->to->user);
    }
    else
        log_debug(ZONE, "[AIM] Already have buddy %s in session %s\n", jp->to->user, jid_full(jp->from));

    xmlnode_free(jp->x);
    return 1;
}

int at_parse_oncoming(aim_session_t *ass, 
                      aim_frame_t *command, ...)
{

    xmlnode away;
    xmlnode status;
	xmlnode x;
	at_session s;
    at_buddy buddy;
    ati ti;
    jpacket jp;
    char *msg;
    aim_userinfo_t *userinfo;
    int was_away;

    va_list ap;
    va_start(ap, command);
    userinfo = va_arg(ap, aim_userinfo_t *);
    va_end(ap);

    log_debug(ZONE, "Oncoming buddy %s", userinfo->sn);

	s = (at_session)ass->aux_data;
    ti = s->ti;

    buddy = xhash_get(s->buddies, at_normalize(userinfo->sn));
    if(buddy == NULL)
    {
        jid jtmp;

        buddy = pmalloco(s->p, (sizeof(_at_buddy)));
        buddy->full = jid_new(s->p, ti->i->id);
        jid_set(buddy->full, userinfo->sn, JID_USER);
        buddy->last = xmlnode_new_tag_pool(s->p,"query");
        buddy->is_away = -1;
        xmlnode_put_attrib(buddy->last,"xmlns",NS_LAST);
        xmlnode_put_attrib(buddy->last,"stamp",jutil_timestamp());

        xhash_put(s->buddies, buddy->full->user, buddy);
    }
    if(buddy->login_time == 0)
    {
        buddy->login_time = userinfo->onlinesince;
    }
    buddy->idle_time = userinfo->idletime;

    was_away = buddy->is_away;
    if(s->icq)
        buddy->is_away = userinfo->icqinfo.status;
    else
        buddy->is_away = (userinfo->flags&AIM_FLAG_AWAY) != 0;

    if(((buddy->is_away == 0) || s->icq) && (buddy->is_away != was_away))
    {
        char *status_msg;
        char *show;

        x = xmlnode_new_tag("presence");
	xmlnode_put_attrib(x, "to", jid_full(jid_user(s->cur)));
    	xmlnode_put_attrib(x, "from", ti->i->id);
	    jp = jpacket_new(x);
    	jid_set(jp->from, at_normalize(userinfo->sn), JID_USER);
	    xmlnode_put_attrib(jp->x, "from", jid_full(jp->from));
        if((!s->icq) || (s->icq && (buddy->is_away == 0)))
        {
            status = xmlnode_insert_tag(x, "status");
            status_msg = pmalloco(xmlnode_pool(x), 30);
            if(!s->icq)
                sprintf(status_msg, "Online (Idle %d Seconds)", buddy->idle_time);
            else
                sprintf(status_msg, "Online");
        }
        else
        {
            status = xmlnode_insert_tag(x, "show");
            show = pmalloco(xmlnode_pool(x), 30);
            if(buddy->is_away&AIM_ICQ_STATE_CHAT)
                sprintf(show, "chat");
            else if(buddy->is_away&AIM_ICQ_STATE_OCCUPIED)
                sprintf(show, "dnd");
            else if(buddy->is_away&AIM_ICQ_STATE_NA)
                sprintf(show, "xa");
            else if(buddy->is_away&AIM_ICQ_STATE_DND)
                sprintf(show, "dnd");
            else if(buddy->is_away&AIM_ICQ_STATE_AWAY)
                sprintf(show, "away");
            else
                sprintf(show, "xa");
            xmlnode_insert_cdata(status, show, -1);
            status = xmlnode_insert_tag(x, "status");
            status_msg = pmalloco(xmlnode_pool(x), 30);
            if(buddy->is_away&AIM_ICQ_STATE_NA)
                sprintf(status_msg, "not available");
            else if(buddy->is_away&AIM_ICQ_STATE_OCCUPIED &&
                    !(buddy->is_away&AIM_ICQ_STATE_DND))
                sprintf(status_msg, "occupied");
            else
                sprintf(status_msg, "%s", show);
        }
        xmlnode_insert_cdata(status, status_msg, -1);
        at_deliver(ti,jp->x);
    } 
    else if(buddy->is_away == 1 && (buddy->is_away != was_away))
    {
        log_debug(ZONE, "[AT] Requesting Away message for %s", userinfo->sn);
        aim_getinfo(ass, command->conn, userinfo->sn, AIM_GETINFO_AWAYMESSAGE);
    }

	return 1;
}

int at_parse_offgoing(aim_session_t *ass, 
                      aim_frame_t *command, ...)
{

	xmlnode x;
	jpacket jp;
    at_buddy buddy;
	at_session s;
    ati ti;
    char *sn;
    aim_userinfo_t *userinfo;
    
    va_list ap;
    va_start(ap, command);
    userinfo = va_arg(ap, aim_userinfo_t *);
    va_end(ap);
            
	s = (at_session)ass->aux_data;
    ti = s->ti;
    sn = at_normalize(userinfo->sn);
    buddy = (at_buddy)xhash_get(s->buddies, sn);
    if(buddy == NULL)
    {
        jid jtmp;

        buddy = pmalloco(s->p, (sizeof(_at_buddy)));
        buddy->full = jid_new(s->p, ti->i->id);
        jid_set(buddy->full, sn, JID_USER);
        buddy->last = xmlnode_new_tag_pool(s->p,"query");
        xmlnode_put_attrib(buddy->last,"xmlns",NS_LAST);
        xmlnode_put_attrib(buddy->last,"stamp",jutil_timestamp());

        xhash_put(s->buddies, buddy->full->user, buddy);
    }

    buddy->is_away = -1;

    xmlnode_put_attrib(buddy->last,"stamp",jutil_timestamp());    

	x = xmlnode_new_tag("presence");
	xmlnode_put_attrib(x, "to", jid_full(jid_user(s->cur)));
	xmlnode_put_attrib(x, "from", ti->i->id);
	xmlnode_put_attrib(x, "type","unavailable");
	jp = jpacket_new(x);
	jid_set(jp->from, at_normalize(sn), JID_USER);
	xmlnode_put_attrib(jp->x, "from", jid_full(jp->from));

    at_deliver(ti,jp->x);

	return 1;
}

int at_parse_evilnotify(aim_session_t *sess, 
                              aim_frame_t *command, ...)
{
    va_list ap;
    xmlnode x;
    xmlnode body;
    jpacket jp;
    at_session s;
    ati ti;
    char msg[100];
    int newevil;
    aim_userinfo_t *userinfo;

    va_start(ap, command);
    newevil = va_arg(ap, int);
    userinfo = va_arg(ap, aim_userinfo_t *);
    va_end(ap);

    memset(msg, '\0', 100);
    snprintf(msg, 100, "Warning from: %s (new level: %2.1f%%", 
             (userinfo && strlen(userinfo->sn))?userinfo->sn:"anonymous",
             ((float)newevil)/10);

    s = (at_session)sess->aux_data;
    ti = s->ti;

	x = xmlnode_new_tag("message");
	xmlnode_put_attrib(x, "to", jid_full(s->cur));
	xmlnode_put_attrib(x, "from", jid_full(s->from));
	xmlnode_put_attrib(x, "type","error");
	body = xmlnode_insert_tag(x, "error");
    xmlnode_insert_cdata(body, (char *)&msg, strlen((char *)&msg));

    jp = jpacket_new(x);
    at_deliver(ti,jp->x);

    return 1;
}

int at_parse_userinfo(aim_session_t *sess, 
                            aim_frame_t *command, ...)
{
    aim_userinfo_t *userinfo;
    char *prof_encoding = NULL;
    char *prof = NULL;
    unsigned short inforeq = 0;
    xmlnode x;
    xmlnode show;
    xmlnode status;
    jpacket jp;
    at_session s;
    ati ti;

    va_list ap;
    va_start(ap, command);
    userinfo = va_arg(ap, aim_userinfo_t *);
    prof_encoding = va_arg(ap, char *);
    prof = va_arg(ap, char *);
    inforeq = va_arg(ap, int);
    va_end(ap);

    s = (at_session)sess->aux_data;
    ti = s->ti;

    if (inforeq == AIM_GETINFO_GENERALINFO) {
        /*
         printf("faimtest: userinfo: profile_encoding: %s\n", prof_encoding ? prof_encoding : "[none]");
        printf("faimtest: userinfo: prof: %s\n", prof ? prof : "[none]");
        */
    } else if (inforeq == AIM_GETINFO_AWAYMESSAGE) {
        x = xmlnode_new_tag("presence");
	xmlnode_put_attrib(x, "to", jid_full(jid_user(s->cur)));
    	xmlnode_put_attrib(x, "from", ti->i->id);
	    jp = jpacket_new(x);
    	jid_set(jp->from, at_normalize(userinfo->sn), JID_USER);
	    xmlnode_put_attrib(jp->x, "from", jid_full(jp->from));
        show = xmlnode_insert_tag(x, "show");
        xmlnode_insert_cdata(show, "away", -1);
        status = xmlnode_insert_tag(x, "status");
        if(prof != NULL)
        {
		char *p1, *p2;
		char charset[32];
		int len;
		char *utf8_str;
		utf8_str = malloc(8192);

		// obtain charset
		charset[0] = 0;
		p1 = strstr(prof_encoding, "charset=");
		if(p1) {
			p1 += 8;
			if(*p1 == '\"') {
				++p1;
				p2 = strchr(p1, '\"');
				if(p2) {
					len = p2 - p1;
					if(len < 32) {
						strncpy(charset, p1, len);
						charset[len] = 0;
					}
				}
			}
		}

		// not utf8?
		if(strcmp(charset, "utf-8"))
			prof = str_to_UTF8(jp->p, prof);

		if(!s->icq) {
			msgconv_aim2plain(prof, utf8_str, 8192);
			prof = utf8_str;
		}
            xmlnode_insert_cdata(status, prof, -1);
		free(utf8_str);
        } else {
            xmlnode_insert_cdata(status, "Away", -1);
        }
        at_deliver(ti,jp->x);
    } else 
        log_debug(ZONE, "[AT] userinfo: unknown info request");

    return 1;
}

#define VCARD_ADD(q, parent, name, value) \
    xmlnode_insert_cdata(xmlnode_insert_tag(xmlnode_insert_tag(q,parent),name), \
    it_convert_windows2utf8(p,value),-1)

int at_parse_icq_simpleinfo(aim_session_t *sess,
                            aim_frame_t *command, ...)
{
	va_list ap;
	struct aim_icq_simpleinfo *info;
	jpacket jp;
	at_session s;
	xmlnode t, q;
	pool p;

	s = (at_session)sess->aux_data;

	va_start(ap, command);
	info = va_arg(ap, struct aim_icq_simpleinfo *);
	va_end(ap);

	jp = s->icq_vcard_response;
	if(!jp)
	    log_debug(ZONE, "[AT] got icq_simpleinfo without request, dropped");

	q = jp->iq;
	p = jp->p;

	t = xmlnode_insert_tag(q,"FN");
	if (info->first)
	{
	    /* we have a first name - if we have a last name, we have to concatenate it */
	    if (info->last)
	        xmlnode_insert_cdata(t,it_convert_windows2utf8(p,spools(p,info->first," ",info->last,p)),-1);
	    else
	        xmlnode_insert_cdata(t,it_convert_windows2utf8(p,info->first),-1);
	}
	else if (info->last)
	    xmlnode_insert_cdata(t,it_convert_windows2utf8(p,info->last),-1);

	t = xmlnode_insert_tag(q,"N");
	if (info->first)
	    xmlnode_insert_cdata(xmlnode_insert_tag(t,"GIVEN"),it_convert_windows2utf8(p,info->first),-1);
	if (info->last)
	    xmlnode_insert_cdata(xmlnode_insert_tag(t,"FAMILY"),it_convert_windows2utf8(p,info->last),-1);
	if (info->nick)
	    xmlnode_insert_cdata(xmlnode_insert_tag(q,"NICKNAME"),it_convert_windows2utf8(p,info->nick),-1);
	if (info->email)
	{
	    t = VCARD_ADD(q,"EMAIL","USERID",info->email);
	    xmlnode_insert_tag(t,"INTERNET");
	    xmlnode_insert_tag(t,"PREF");
	}

	at_deliver(s->ti,jp->x);

	s->icq_vcard_response = NULL;

	return 1;
}

int at_parse_locerr(aim_session_t *sess, 
                          aim_frame_t *command, ...)
{
/* XXX Do we want this?
    va_list ap;
    char *destsn;
    unsigned short reason;
    char *sn;
    xmlnode x;
    xmlnode body;
    jpacket jp;
    session s;
    char msg[1024];

    va_start(ap, command);
    reason = va_arg(ap, unsigned short);
    destsn = va_arg(ap, char *);
    va_end(ap);

    memset(&msg, '\0', 1024);
    snprintf("User information for %s unavailable (reason 0x%04x: %s)\n", destsn, reason, (reason<msgerrreasonslen)?msgerrreasons[reason]:"unknown");

    s = (session)ass->aux_data;

	x = xmlnode_new_tag("message");
	xmlnode_put_attrib(x, "to", jid_full(s->j));
	xmlnode_put_attrib(x, "from", aimtrans__instance->id);
	xmlnode_put_attrib(x, "type","error");
	body = xmlnode_insert_tag(x, "error");
    xmlnode_insert_cdata(body, &msg, strlen(&msg));

    jp = jpacket_new(x);
	ehandler_send(ehandler_at, jp->x, s->j->server);
*/  
    return 1;
}
