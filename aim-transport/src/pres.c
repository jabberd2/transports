/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"

/** Handle received presence packets that do not correspond to an
    active session */
int at_server_pres(ati ti, jpacket jp)
{
    xmlnode x;

    switch(jpacket_subtype(jp))
    {
        case JPACKET__AVAILABLE:
            if(jp->to->user != NULL)
            {
	      // if presence sent to a specific user, queue it for now
                return at_buddy_add(ti, jp);
            } else {
	      // if presence sent to (aimtrans) server, start a session.
                log_debug(ZONE, "[AT] Starting a new session!\n");
                return at_auth_user(ti, jp);
            }
        case JPACKET__PROBE:
            if(jp->to->user != NULL)
                return at_buddy_add(ti, jp);
            else {
                xmlnode_free(jp->x);
                return 1;
            }
        case JPACKET__UNAVAILABLE:
            log_debug(ZONE, "[AT] Unavailable sent to server");
            xmlnode_free(jp->x);
            return 1;
        default:
            jutil_error(jp->x, TERROR_NOTACCEPTABLE);
            at_deliver(ti,jp->x);
            return 1;
    }

    xmlnode_free(jp->x);
    return 0;
}

/** Handle a received presence packet when there *is* an active session
    (c.v. at_server_pres()) */
int at_session_pres(at_session s, jpacket jp)
{
    ati ti;
    xmlnode pres;
    xmlnode show;
    xmlnode x;
    jid j;
    pool p;
    char profile[] = "";
    char *show_data;
    char *status_data;

    ti = s->ti;

    if(s->exit_flag)
    {
        xmlnode_free(jp->x);
        return 1;
    }

    switch(jpacket_subtype(jp))
    {
        case JPACKET__PROBE:
            log_debug(ZONE, "[AT] Probed, no logical way to handle, eh? %s",jp->to->user);
            at_send_buddy_presence(s,jp->to->user);                
            xmlnode_free(jp->x);
            return 1;
        case JPACKET__UNAVAILABLE:
            log_debug(ZONE, "[AT] Unavailabe sent to session");
            /* insert into ppdb, check to see if we should go offline */
            s->at_PPDB=ppdb_insert(s->at_PPDB,jp->from,jp->x);
            log_debug(ZONE, "[AT] Checking at_PPDB for %s", jid_full(s->cur));
            p = pool_new();
            j = jid_new(p, jid_full(s->cur));
            jid_set(j, NULL, JID_RESOURCE);
            x=ppdb_primary(s->at_PPDB,j);
            pool_free(p);
            if(x!=NULL)
            {
                /* not last resource, don't go offline yet */
                s->cur = jid_new(s->p,xmlnode_get_attrib(x, "from"));
                log_debug(ZONE,"[AT] active resources(%s), not ending session", jid_full(s->cur));
                xmlnode_free(jp->x);
                return 1;
            }
			log_debug(ZONE, "[AT] Telling the session to end!");
            s->exit_flag = 1;
            xmlnode_free(jp->x);
            return 1;
		case JPACKET__AVAILABLE:
				if(jp->to->user != NULL)
				{
					return at_buddy_add(ti, jp);
                }

                /* If we're not logged in they haven't recved initial pres */
                if (!s->loggedin)
                {
                    return 0;
                }

		/* received a presence notification from one of
		   possibly many resources. Store this presence in the
		   presence db, then select the primary presence
		   (latest report from a resource with the highest
		   priority) and report that to AOL. */
                s->at_PPDB=ppdb_insert(s->at_PPDB,jp->from,jp->x);
                pres = ppdb_primary(s->at_PPDB, jid_user(s->cur));
                s->cur = jid_new(s->p, xmlnode_get_attrib(pres, "from"));
                show_data = xmlnode_get_tag_data(pres, "show");
                status_data = xmlnode_get_tag_data(pres, "status");
                if(s->status != NULL)
                {
                    free(s->status);
                    s->status = NULL;
                }
                if(status_data == NULL)
                {
                    s->status = strdup(profile);
                } else {
                    s->status = strdup(status_data);
                }
                if(show_data == NULL || j_strcmp(show_data, "chat") == 0)
                {
                    char *convstr = malloc(8192);
                    if(!s->icq)
                        msgconv_plain2aim(s->status, convstr, 8192);
                    else
                        strcpy(convstr, s->status);

                    s->away = 0;
                    aim_bos_setprofile(s->ass, 
                        aim_getconn_type(s->ass, AIM_CONN_TYPE_BOS),
                        profile, "", AIM_CAPS_CHAT);
                    x = jutil_presnew(JPACKET__AVAILABLE, jid_full(jid_user(s->cur)), "Online");
                    xmlnode_put_attrib(x, "from", jid_full(s->from));
                    at_deliver(ti,x);
                    xmlnode_free(jp->x);

                    if(s->icq) {
                        if(j_strcmp(show_data, "chat") == 0) {
                          aim_setextstatus(s->ass, aim_getconn_type(s->ass,
                                           AIM_CONN_TYPE_BOS),AIM_ICQ_STATE_CHAT);
                        }
                        else {
                          aim_setextstatus(s->ass, aim_getconn_type(s->ass,
                                     AIM_CONN_TYPE_BOS),AIM_ICQ_STATE_ONLINE);
                        }
                    }

                    free(convstr);
                    return 1;
                } else {
                    char *convstr = malloc(8192);
                    if(!s->icq)
                        msgconv_plain2aim(s->status, convstr, 8192);
                    else
                        strcpy(convstr, s->status);

                    log_debug(ZONE, "[AT] Setting user away");
                    s->away = 1;
		    s->awaysetat = time(NULL);  /*each away msg has a unique timestamp  */
                    aim_bos_setprofile(s->ass, 
                            aim_getconn_type(s->ass, AIM_CONN_TYPE_BOS),
                            profile, convstr, AIM_CAPS_CHAT);
		    x = jutil_presnew(JPACKET__AVAILABLE, jid_full(jid_user(s->cur)), s->status);
                    if(s->icq) {
                    aim_setextstatus(s->ass, aim_getconn_type(s->ass,
                                     AIM_CONN_TYPE_BOS),AIM_FLAG_AWAY);
                    }

                    show = xmlnode_insert_tag(x, "show");
                    if(s->icq) {
                        xmlnode_insert_cdata(show, show_data, -1);
                    }
                    else {
                        xmlnode_insert_cdata(show, "away", -1);
                    }
                    xmlnode_put_attrib(x, "from", jid_full(s->from));
                    log_debug(ZONE, "[AT] Pres Send: %s", xmlnode2str(x));
                    at_deliver(ti,x);
                    xmlnode_free(jp->x);

                    if(s->icq) {
                    if(j_strcmp(show_data, "away") == 0)
                          aim_setextstatus(s->ass, aim_getconn_type(s->ass,
                                           AIM_CONN_TYPE_BOS),AIM_ICQ_STATE_AWAY);
                    else if(j_strcmp(show_data, "dnd") == 0)
                          aim_setextstatus(s->ass, aim_getconn_type(s->ass,
                                           AIM_CONN_TYPE_BOS),AIM_ICQ_STATE_DND);
                    else if(j_strcmp(show_data, "xa") == 0)
                          aim_setextstatus(s->ass, aim_getconn_type(s->ass,
                                           AIM_CONN_TYPE_BOS),AIM_ICQ_STATE_NA);
                    }

                    free(convstr);
                    return 1;
                }

                xmlnode_free(jp->x);
                return 1;
        default:
            xmlnode_free(jp->x);
            return 1;
    }

    return 0;
}

/** Handle probes so that multiple resources may connect. */
void at_send_buddy_presence(at_session s, char *user)
{
	xmlnode status;
	xmlnode x;
	at_buddy buddy;
	ati ti;
	jpacket jp;
        char *status_msg;
        char *show;

	ti = s->ti;

	buddy = xhash_get(s->buddies, at_normalize(user));

	if(buddy==NULL)
	{
		log_debug(ZONE,"Not found: %s",user);
		return;
	}

	if(buddy->is_away==-1)
	{
		log_debug(ZONE,"%s is -1 (%d)",user,buddy->is_away);
		return;
	}
	else
	log_debug(ZONE,"Found: %s",user);
	//From the if

        x = xmlnode_new_tag("presence");
        xmlnode_put_attrib(x, "to", jid_full(jid_user(s->cur)));
        xmlnode_put_attrib(x, "from", ti->i->id);

        jp = jpacket_new(x);
        jid_set(jp->from, at_normalize(buddy->full->user), JID_USER);
        xmlnode_put_attrib(jp->x, "from", jid_full(jp->from));
	 
        if(buddy->is_away == 0)
        {
            status = xmlnode_insert_tag(x, "status");
            status_msg = pmalloco(xmlnode_pool(x), 30);
            if(!s->icq)
                sprintf(status_msg, "Online (Idle %d Seconds)", buddy->idle_time);
            else
                sprintf(status_msg, "Online");
            xmlnode_insert_cdata(status, status_msg, -1);
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
            xmlnode_insert_cdata(status, status_msg, -1);
        }
        at_deliver(ti,jp->x);
        log_debug(ZONE,"Sent presence for %s",jid_full(jp->from));
}

