/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "License").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the License.  You
 * may obtain a copy of the License at http://www.jabber.com/license/ or at
 * http://www.opensource.org/.  
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Copyrights
 * 
 * Portions created by or assigned to Jabber.com, Inc. are 
 * Copyright (c) 1999-2000 Jabber.com, Inc.  All Rights Reserved.  Contact
 * information for Jabber.com, Inc. is available at http://www.jabber.com/.
 *
 * Portions Copyright (c) 1998-1999 Jeremie Miller.
 * 
 * Acknowledgements
 * 
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 * 
 * --------------------------------------------------------------------------*/
#include "jabberd.h"


/* WP changes */
/*
 
 - Added sems
 - xdb_shutdown finishes xdb_get, xdb_set queries ( if they were by LAN )

*/

result xdb_results(instance id, dpacket p, void *arg)
{
    xdbcache xc = (xdbcache)arg;
    xdbcache curx;
    int idnum;
    char *idstr;

    if(p->type != p_NORM || *(xmlnode_get_name(p->x)) != 'x') return r_PASS; /* yes, we are matching ANY <x*> element */

    log_debug(ZONE,"xdb_results checking xdb packet %s",xmlnode2str(p->x));

    if((idstr = xmlnode_get_attrib(p->x,"id")) == NULL) return r_ERR;

    idnum = atoi(idstr);

    pthread_mutex_lock(&(xc->sem));
    for(curx = xc->next; curx->id != idnum && curx != xc; curx = curx->next); /* spin till we are where we started or find our id# */

    /* we got an id we didn't have cached, could be a dup, ignore and move on */
    if(curx->id != idnum)
    {
	  pool_free(p->p);
	  pthread_mutex_unlock(&(xc->sem));
	  return r_DONE;
    }

    /* associte only a non-error packet w/ waiting cache */
    if(j_strcmp(xmlnode_get_attrib(p->x,"type"),"error") == 0) {
	  curx->data = NULL;
	  pool_free(p->p); // very bad memory leak
	} 
	else
	  curx->data = p->x;

    /* remove from ring */
    curx->prev->next = curx->next;
    curx->next->prev = curx->prev;
    pthread_mutex_unlock(&(xc->sem));


    /* free the thread! */
    curx->preblock = 1;

    return r_DONE; /* we processed it */
}

typedef struct xdb_resend_struct {
  instance i;
  xmlnode x;
} *xdb_resend, _xdb_resend;

/** Resend packet in other thread  */
void resend_xdb(void *arg) {
  xdb_resend cur = (xdb_resend)arg;
  deliver(dpacket_new(cur->x), cur->i);
}

/* actually deliver the xdb request */
void xdb_deliver(instance i, xdbcache xc, int another_thread)
{
    xmlnode x;
    char ids[15];

    x = xmlnode_new_tag("xdb");

    if(xc->set)
    {
        xmlnode_put_attrib(x,"type","set");
        xmlnode_insert_tag_node(x,xc->data); /* copy in the data */
        if(xc->act != NULL)
            xmlnode_put_attrib(x,"action",xc->act);
        if(xc->match != NULL)
            xmlnode_put_attrib(x,"match",xc->match);
    }
	else {
	  xmlnode_put_attrib(x,"type","get");
	}
    xmlnode_put_attrib(x,"to",jid_full(xc->owner));
    xmlnode_put_attrib(x,"from",i->id);
    xmlnode_put_attrib(x,"ns",xc->ns);
    sprintf(ids,"%d",xc->id);
    xmlnode_put_attrib(x,"id",ids); /* to track response */

	if (another_thread) {
	  xdb_resend cur = pmalloco(xmlnode_pool(x),sizeof(_xdb_resend));
	  cur->x = x;
	  cur->i = i;
	  mtq_send(NULL,NULL,resend_xdb,(void *)cur);
	}
	else {
	  deliver(dpacket_new(x), i);
	}
}


void xdb_shutdown(void *arg)
{
    xdbcache xc = (xdbcache)arg;
    xdbcache cur, next;

	log_debug(ZONE,"XDB SHUTDOWN");

    pthread_mutex_lock(&(xc->sem));

    cur = xc->next;
    while(cur != xc)
	  {
        next = cur->next;
		
		/* remove from ring */
		cur->prev->next = cur->next;
		cur->next->prev = cur->prev;
		
		/* make sure it's null as a flag for xdb_set's */
		cur->data = NULL;
		
		/* free the thread! */
	    cur->preblock = 1;
		
		cur = next;
		continue;
	  }
	
    pthread_mutex_unlock(&(xc->sem));
}

/** resend packets. heartbeat thread */
result xdb_thump(void *arg)
{
    xdbcache xc = (xdbcache)arg;
    xdbcache cur, next;
    int now = time(NULL);

    pthread_mutex_lock(&(xc->sem));
    /* spin through the cache looking for stale requests */
    cur = xc->next;
    while(cur != xc)
    {
        next = cur->next;

        /* really old ones get wacked */
        if((now - cur->sent) > 30)
        {
            /* remove from ring */
            cur->prev->next = cur->next;
            cur->next->prev = cur->prev;

            /* make sure it's null as a flag for xdb_set's */
            cur->data = NULL;

            /* free the thread! */
			cur->preblock = 1;

            cur = next;
            continue;
        }

        /* resend the waiting ones every so often */
        if((now - cur->sent) > 10) {
		  log_alert(ZONE,"Resend xdb packet");
	 	  xdb_deliver(xc->i, cur,1);
		}

        /* cur could have been free'd already on it's thread */
        cur = next;
    }


    pthread_mutex_unlock(&(xc->sem));
    return r_DONE;
}

xdbcache xdb_cache(instance id)
{
    xdbcache newx;

    if(id == NULL)
    {
        fprintf(stderr, "Programming Error: xdb_cache() called with NULL\n");
        return NULL;
    }

    newx = pmalloco(id->p, sizeof(_xdbcache));
    newx->i = id; /* flags it as the top of the ring too */
    newx->next = newx->prev = newx; /* init ring */
    pthread_mutex_init(&(newx->sem),NULL);

    /* register the handler in the instance to filter out xdb results */
    register_phandler(id, o_PRECOND, xdb_results, (void *)newx);

    /* heartbeat to keep a watchful eye on xdb_cache */
    register_beat(10,xdb_thump,(void *)newx);

    register_shutdown_first(xdb_shutdown, (void *)newx);

	pool_cleanup(id->p, xdb_shutdown, (void*)newx);
	
    return newx;
}

/* blocks until namespace is retrieved, host must map back to this service! */
xmlnode xdb_get(xdbcache xc, jid owner, char *ns)
{
    xdbcache newx;
    xmlnode x;
	pool p;

    if(xc == NULL || owner == NULL || ns == NULL)
    {
        fprintf(stderr,"Programming Error: xdb_get() called with NULL\n");
        return NULL;
    }

    log_debug(ZONE,"XDB GET");

    /* init this newx */
	p = pool_new();
	newx = pmalloco(p, sizeof(_xdbcache));
    newx->i = xc->i;
    newx->set = 0;
    newx->data = NULL;
    newx->ns = ns;
    newx->owner = owner;
    newx->sent = time(NULL);
    newx->preblock = 0; /* flag */


    pthread_mutex_lock(&(xc->sem));
    newx->id = xc->id++;
    newx->next = xc->next;
    newx->prev = xc;
    newx->next->prev = newx;
    xc->next = newx;
    pthread_mutex_unlock(&(xc->sem));

    /* send it on it's way */
    xdb_deliver(xc->i, newx,0);

    /* if it hasn't already returned, we should block here until it returns */
    while (newx->preblock != 1) usleep(10);

    /* newx.data is now the returned xml packet */

    /* return the xmlnode inside <xdb>...</xdb> */
    for(x = xmlnode_get_firstchild(newx->data); x != NULL && xmlnode_get_type(x) != NTYPE_TAG; x = xmlnode_get_nextsibling(x));

    /* there were no children (results) to the xdb request, free the packet */
    if(x == NULL)
        xmlnode_free(newx->data);

	pool_free(p);

    return x;
}

/* sends new xml xdb action, data is NOT freed, app responsible for freeing it */
/* act must be NULL or "insert" for now, insert will either blindly insert data into the parent (creating one if needed) or use match */
/* match will find a child in the parent, and either replace (if it's an insert) or remove (if data is NULL) */
int xdb_act(xdbcache xc, jid owner, char *ns, char *act, char *match, xmlnode data)
{
    xdbcache newx;
	pool p;

    if(xc == NULL || owner == NULL || ns == NULL)
    {
        fprintf(stderr,"Programming Error: xdb_set() called with NULL\n");
        return 1;
    }

    
    log_debug(ZONE,"XDB SET");

    /* init this newx */
	p = pool_new();
	newx = pmalloco(p, sizeof(_xdbcache));
    newx->i = xc->i;
    newx->set = 1;
    newx->data = data;
    newx->ns = ns;
    newx->act = act;
    newx->match = match;
    newx->owner = owner;
    newx->sent = time(NULL);
    newx->preblock = 0; /* flag */


    pthread_mutex_lock(&(xc->sem));
    newx->id = xc->id++; 
    newx->next = xc->next;
    newx->prev = xc;
    newx->next->prev = newx;
    xc->next = newx; 
    pthread_mutex_unlock(&(xc->sem));

    /* send it on it's way */
    xdb_deliver(xc->i, newx,0);

    /* if it hasn't already returned, we should block here until it returns */
    while (newx->preblock != 1) usleep(10);


    /* if it didn't actually get set, flag that */
    if(newx->data == NULL) {
	  pool_free(p);
	  return 1;
	}

    xmlnode_free(newx->data);

	pool_free(p);

    return 0;
}

/* sends new xml to replace old, data is NOT freed, app responsible for freeing it */
int xdb_set(xdbcache xc, jid owner, char *ns, xmlnode data)
{
    return xdb_act(xc, owner, ns, NULL, NULL, data);
}



