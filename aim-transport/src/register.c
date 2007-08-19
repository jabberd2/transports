/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"

/*
 * XXX ICK I entered fast mode and just browsed this for errors.  Comb me again
 */
int at_register(ati ti, jpacket jp)
{
	xmlnode query;
	char *user;
	char *pass;
	at_session s;
	aim_session_t *ass;
    xmlnode info;
    xmlnode sn;
    char *key;
    xmlnode dup;
	
    switch(jpacket_subtype(jp))
    {
        case JPACKET__GET:

            log_debug(ZONE, "[AIM] Handling register:get.\n");
	        query = xmlnode_get_tag(jp->x, "query");

            xmlnode_insert_cdata(xmlnode_insert_tag(query,"key"),
                                 jutil_regkey(NULL,jid_full(jp->from)),-1);
            sn = xmlnode_insert_tag(query,"username");
            s = at_session_find_by_jid(ti, jp->from);
            if(s!=NULL)
            {
                ass = s->ass;
                xmlnode_insert_cdata(sn, ass->sn, 
							         strlen(ass->sn));
                xmlnode_insert_tag(query, "registered");
            }
            xmlnode_insert_tag(query,"password");
            info = xmlnode_insert_tag(query, "instructions");
            xmlnode_insert_cdata(info, "Enter your AIM screenname or ICQ UIN and the password for that account", -1);
            xmlnode_put_attrib(jp->x,"type","result");
            jutil_tofrom(jp->x);

            break;
        case JPACKET__SET:
            log_debug(ZONE, "[AIM] Handling register:set.\n");
            if(xmlnode_get_tag(xmlnode_get_tag(jp->x,"query"),"remove") != NULL)
            {
                char *from,*to;
                at_buddy cur;
                xmlnode sub;

                log_debug(ZONE, "[AIM] Removing registration\n");

                s = at_session_find_by_jid(ti, jp->from);

                if(s == NULL)
                {
                    jutil_error(jp->x, TERROR_REGISTER);
                    break;
                }
                s->exit_flag = 1;
                xhash_walk(s->buddies, _at_buddies_unsubscribe, s);
                sub = jutil_presnew(JPACKET__UNSUBSCRIBE, jid_full(jp->from), 
                                    NULL);
                xmlnode_put_attrib(sub, "from", jid_full(s->from));
                at_deliver(ti,sub);
                sub = jutil_iqresult(jp->x);
                at_deliver(ti,sub);
            
                return 1;
            }
            /* make sure they sent a valid */
            key = xmlnode_get_tag_data(jp->iq,"key");
            xmlnode_hide(xmlnode_get_tag(jp->iq,"key"));
            if(key == NULL || jutil_regkey(key,jid_full(jp->from)) == NULL)
            {
                jutil_error(jp->x, TERROR_NOTACCEPTABLE);
                break;
            }
            
	    	query = xmlnode_get_tag(jp->x, "query");

		    user = xmlnode_get_data(xmlnode_get_firstchild(
                                    xmlnode_get_tag(query,"username")));
            pass = xmlnode_get_data(xmlnode_get_firstchild(
                                    xmlnode_get_tag(query,"password")));
            xmlnode_hide(xmlnode_get_tag(jp->x,"query"));
                
            if(user == NULL || pass == NULL)
            {
                xmlnode err;
			
                log_debug(ZONE, "[AIM] Handling register:err.\n");
                jutil_error(jp->x, TERROR_NOTACCEPTABLE);
                break;
            }
            else
            {
                xmlnode logon;

                logon = xmlnode_new_tag("logon");
                xmlnode_put_attrib(logon, "id", user);
                xmlnode_put_attrib(logon, "pass", pass);
                
                log_debug(ZONE, "[AT] Attempting to start a session from register");

                if(at_session_create(ti, logon, jp)!=NULL)
                {
                    log_debug(ZONE, "[AT] Subscribing to user from register");
                    at_auth_subscribe(ti, jp);

                    at_xdb_set(ti, jp->to->server, jp->from, logon, AT_NS_AUTH);

                    jutil_iqresult(jp->x);
                    break;
                } else {
                    xmlnode err;
			
                    log_debug(ZONE, "[AT] Unable to start session");
                    jutil_error(jp->x, TERROR_NOTACCEPTABLE);
                    break;
                }
            }
            break;
        default:
            log_debug(ZONE, "[AIM] Odd we didn't handle this jpacket for subtype %d", jpacket_subtype(jp));
            return 0;
    }
    log_debug(ZONE, "[AIM] Sending %s as iq reply\n", xmlnode2str(jp->x));
    at_deliver(ti,jp->x);
	
    return 1;
}
