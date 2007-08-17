/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 *  Helper routines */

#include "jit/icqtransport.h"

jid it_xdb_id(pool p, jid id, char *server)
{
    return jid_new(p,spools(p,id->user,"%",id->server,"@",server,p));
}

/* convert pre-2003 XDB data to new format */
void it_xdb_convert(iti ti, char *user, jid nid)
{
    jid oid, old, new;
    xmlnode x;
    pool p;

    if(user == NULL) return;

    p = pool_new();
    oid = jid_new(p, user);

    if(oid->user == NULL) return;

    log_debug(ZONE, "Trying to convert XDB for user %s", user);

    old = jid_new(p, spools(p, oid->user, "%", oid->server,
                  "@", ti->i->id, p));
    new = jid_new(p, spools(p, nid->user, "%", nid->server,
                  "@", ti->i->id, p));

    x = xdb_get(ti->xc, old, NS_REGISTER);
    if(x != NULL)
      if(xdb_set(ti->xc, new, NS_REGISTER, x) == 0)
      {
        xdb_set(ti->xc, old, NS_REGISTER, NULL);
        log_record("convertregistration", "", "", ";%s;",
                   user);
      }

    x = xdb_get(ti->xc, old, NS_ROSTER);
    if(x != NULL)
      if(xdb_set(ti->xc, new, NS_ROSTER, x) == 0)
        xdb_set(ti->xc, old, NS_ROSTER, NULL);

    pool_free(p);
}
UIN_t it_strtouin(char *uin)
{
    return uin != NULL ? strtoul(uin,NULL,10): 0;
}

jid it_uin2jid(pool p, UIN_t uin, char *server)
{
    jid id;
    char buffer[16];

    id = (jid) pmalloco(p,sizeof(struct jid_struct));

    id->p = p;
    id->server = pstrdup(p,server);
	
	if (uin == 0) {
	  id->user = pstrdup(p,"unknown");
	}
	else {
	  snprintf(buffer,16,"%lu",uin);
	  id->user = pstrdup(p,buffer);
	}    
    return id;
}

jid it_sms2jid(pool p, char * sms_id, char *server)
{
    jid id;
    id = (jid) pmalloco(p,sizeof(struct jid_struct));

    id->p = p;
    id->server = pstrdup(p,server);
    id->user = pstrdup(p,sms_id);
    
    return id;
}

icqstatus jit_show2status(const char *show)
{
    if (show == NULL)
        return ICQ_STATUS_ONLINE;

    if (j_strcmp(show,"away")==0)
        return ICQ_STATUS_AWAY;
    if (j_strcmp(show,"busy")==0)
        return ICQ_STATUS_OCCUPIED;
    if (j_strcmp(show,"chat")==0)
        return ICQ_STATUS_FREE_CHAT;
    if (j_strcmp(show,"dnd")==0)
        return ICQ_STATUS_DND;
    if (j_strcmp(show,"xa")==0)
        return ICQ_STATUS_NA;

    return ICQ_STATUS_ONLINE;
}

const char * jit_status2show(icqstatus status)
{
  switch (status) { 
     case ICQ_STATUS_AWAY: 
       return "away";
       break; 
     case ICQ_STATUS_DND:
	   return "dnd";
       break;
     case ICQ_STATUS_NA:
	   return "xa";
       break;
     case ICQ_STATUS_OCCUPIED:
	   return "busy";
       break;
     case ICQ_STATUS_FREE_CHAT:
	   return "chat";
       break;
     default:
	   return NULL;
       break;
   }
}

const char * jit_status2fullinfo(icqstatus status)
{
  switch (status) { 
     case ICQ_STATUS_ONLINE: 
       return "online";
       break; 
     case ICQ_STATUS_AWAY: 
       return "away";
       break; 
     case ICQ_STATUS_DND:
	   return "dnd";
       break;
     case ICQ_STATUS_NA:
	   return "xa";
       break;
     case ICQ_STATUS_OCCUPIED:
	   return "busy";
       break;
     case ICQ_STATUS_FREE_CHAT:
	   return "chat";
       break;
     default:
	   return "offline";
       break;
   }
}

char *it_strrepl(pool p, const char *orig, const char *find, const char *replace)
{
    const char *loc;
    char *newstr, *temp, *tempstr;
    unsigned int olen, flen, rlen, i;

    if (!orig || !find || !replace || !p)
        return NULL;  /* oops */

    olen = strlen(orig);
    flen = strlen(find);
    rlen = strlen(replace);

    temp = strstr(orig,find);
    if (!temp)
        return pstrdup(p,orig);

    /* count up any instances of the string we can find */
    i = 0;
    while (temp)
    { /* advance past current instance and get a count */
        temp += flen;
        i++;
        temp = strstr(temp,find);
    }

    /* allocate a buffer for the new string */
    newstr = pmalloc(p,(olen + ((rlen - flen) * i) + 1) * sizeof(char));
    tempstr = newstr;
    loc = orig;

    while((temp = strstr(loc,find))!=NULL)
    { /* copy pieces into the new buffer */
        memcpy(tempstr,loc,(int)(temp - loc));
        tempstr += (int)(temp - loc);

        memcpy(tempstr,replace,rlen);
        tempstr += rlen;

        loc = temp + flen;
    }

    strcpy(tempstr,loc);  /* copy the last piece */

    return newstr;
}

void it_delay(xmlnode x, char *ts)
{
    xmlnode delay;

    delay = xmlnode_insert_tag(x,"x");
    xmlnode_put_attrib(delay,"xmlns",NS_DELAY);
    xmlnode_put_attrib(delay,"from",xmlnode_get_attrib(x,"to"));
    xmlnode_put_attrib(delay,"stamp",ts);
}

int it_reg_set(session s, xmlnode reg)
{
    iti ti = s->ti;
    jid id;
    xmlnode x;
    pool p;

    if (xdata_test(reg,"submit"))
	reg = xdata_convert(reg,NS_REGISTER);
 
    p = xmlnode_pool(reg);
    while ((x = xmlnode_get_tag(reg,"key")) != NULL) xmlnode_hide(x);
    xmlnode_hide(xmlnode_get_tag(reg,"instructions"));

    /* we don't need al NS_REGISTER tags here */
    xmlnode_hide(xmlnode_get_tag(reg,"nick"));
    xmlnode_hide(xmlnode_get_tag(reg,"first"));
    xmlnode_hide(xmlnode_get_tag(reg,"last"));
    xmlnode_hide(xmlnode_get_tag(reg,"email"));

    id = it_xdb_id(p,s->id,s->from->server);

    /* write to XDB */
    if (xdb_set(ti->xc,id,NS_REGISTER,reg))
    {
        log_error(ZONE,"Failed to update registration information");
        return 1;
    }

    return 0;
}

jid jid_canonize(jid a)
{
    jid ret;

    if(a == NULL) return a;

    ret = pmalloco(a->p,sizeof(struct jid_struct));
    ret->p = a->p;
    ret->user = a->user;
    ret->server = a->server;

    return ret;
}

