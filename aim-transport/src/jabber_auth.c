/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"

int at_auth_user(ati ti, jpacket jp)
{
	char *name;
	char *pass;
    xmlnode res;
	at_session s;
    xmlnode x;

    /* get login data */
    x = at_xdb_get(ti, jp->from, AT_NS_AUTH);
    if(x == NULL)
    {
      /* convert pre-2003 (hashed) XDB files */
      at_xdb_convert(ti, xmlnode_get_attrib(jp->x,"origfrom"), jp->from);
      x = at_xdb_get(ti, jp->from, AT_NS_AUTH);
    }

    if(x == NULL)
    {
      xmlnode err, m;

      log_warn(ZONE, "[AT] No auth data for %s found", jid_full(jp->from));

      m = xmlnode_new_tag("message");

      xmlnode_put_attrib(m, "type", "error");
      xmlnode_put_attrib(m, "from", jid_full(jp->to));
      xmlnode_put_attrib(m, "to", jid_full(jp->from));

      err = xmlnode_insert_tag(m, "error");
      xmlnode_put_attrib(err, "code", "407");
      xmlnode_insert_cdata(err, "No logon data found", -1);

      at_deliver(ti,m);
      xmlnode_free(jp->x);

      return 1;
    }

    log_debug(ZONE,"[AT] logging in session");
    at_session_create(ti, x, jp);

    xmlnode_free(x);
    xmlnode_free(jp->x);

    return 1;

}

void at_auth_subscribe(ati ti, jpacket jp)
{
    /* Here we actually subscribe to the user */
    xmlnode x;
    jid jnew;

    x = xmlnode_new_tag("presence");

    jnew = jid_new(xmlnode_pool(x), ti->i->id);
    jid_set(jnew, "registered", JID_RESOURCE);
    
    log_debug(ZONE, "[AIM] Subscribing to %s presence\n", jid_full(jp->from));
	
    xmlnode_put_attrib(x, "to", jid_full(jp->from));
    xmlnode_put_attrib(x, "from", jid_full(jnew));
    xmlnode_put_attrib(x, "type", "subscribe");

    at_deliver(ti,x);

    return;
}
