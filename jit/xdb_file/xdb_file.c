/* --------------------------------------------------------------------------

   xdb_file module ( threaded environment )

   Author: £ukasz Karwacki <lukasm@wp-sa.pl>

 * --------------------------------------------------------------------------*/
 
#include <jabberd.h>

//default value for hash 
#define FILES_PRIME 509

typedef struct cacher_struct
{
    WPHASH_BUCKET;
    volatile xmlnode file;
    unsigned long lasttime; //last access time
    struct cacher_struct *anext,*aprev;
    volatile int ref; //number of references to hash element
} *cacher, _cacher;

typedef struct xdbf_struct
{
    int shutdown;
    char *spool;
    instance i;
    int timeout;    //hash elements timeout 
    HASHTABLE hash;
    int sizelimit;  //sizelimit for set
    SEM_VAR hash_sem;
    cacher last;    //oldest element
    cacher first;   //latest element
    unsigned long actualtime;
} *xdbf, _xdbf;

/*
          | aprev    c  anext |   | aprev   c  anext |
           xf->first ^                      ^  xf->last
*/

inline void cacher_add_element(xdbf xf,cacher c) {
  /* first element */
  if (xf->last == NULL)
	xf->last = c;
  else
	xf->first->aprev = c;

  c->anext = xf->first;
  xf->first = c;
}


inline void cacher_remove_element(xdbf xf, cacher c) {

  if (c->aprev) {
	c->aprev->anext=c->anext;
  }
  if (c->anext) {
	c->anext->aprev=c->aprev;
  }
  if (xf->first == c) {
	xf->first = c->anext;
  }
  if (xf->last == c) {
	xf->last = c->aprev;
  }
  c->anext = c->aprev = NULL;
  return;

}

inline void cacher_touch_element(xdbf xf,cacher c) {
  /* is it add */
  if ( (c->aprev != NULL) || (c->anext != NULL) || (xf->first==c) ) {
	cacher_remove_element(xf,c);
  }
  cacher_add_element(xf,c);
}

/* remove some elements */
result xdb_file_purge(void *arg)
{
    xdbf xf = (xdbf)arg;
	cacher c;
	cacher t;

	log_debug(ZONE,"check hash");

	SEM_LOCK(xf->hash_sem);
    xf->actualtime = time(NULL);

	if ((xf->last == NULL)||(xf->shutdown)) {
	  SEM_UNLOCK(xf->hash_sem);
	  return r_DONE;
	}

	t = NULL;

	/* loop till c and data are valid */
	for (c = xf->last; c != NULL;) {

	  /* if someone is using this */
	  if (c->ref) break;
	  if (c->lasttime > xf->actualtime - xf->timeout) break;

	  /* no ref and old */

	  t = c;
	  
	  c = t->aprev;

	  t->aprev = NULL;
	  t->anext = NULL;
	  
	  /* remove */
	  wpxhash_zap(xf->hash,t->wpkey);

	  /* free */
	  xmlnode_free(t->file);
	}

	/* t elem is free , c is last one */

	/* if something was removed */
	if (t) {
	  /* if not everythink to remove */
	  if (c) {
		xf->last = c;
		c->anext = NULL;
	  }
	  else {
		xf->first = NULL;
		xf->last = NULL;
	  }	  
	}
	SEM_UNLOCK(xf->hash_sem);
    return r_DONE;
}


/* extern */
char *xdb_file_full(int create, pool p, char *spl, char *host, char *file, char *ext);
xmlnode xdb_file_parse(char *file,pool p);
int xdb_file2file(char * file, xmlnode node, int maxsize);

/* the callback to handle xdb packets */
result xdb_file_phandler(instance i, dpacket dp, void *arg)
{
    char *full, *ns;
    xdbf xf = (xdbf)arg;
    xmlnode file, top, data;
    int flag_set = 0;
    cacher c; //element
	int remove = 0;

    log_debug(ZONE,"handling xdb request %s",xmlnode2str(dp->x));
        
    if((ns = xmlnode_get_attrib(dp->x,"ns")) == NULL)
        return r_ERR;

    if(j_strcmp(xmlnode_get_attrib(dp->x,"type"), "set") == 0)
        flag_set = 1;

    /* is this request specific to a user or global data? use that for the file name */
    if(dp->id->user != NULL)
    {
        full = xdb_file_full(flag_set, dp->p, xf->spool, 
							 dp->id->server, dp->id->user, "xml");
    }
    else
	{
		full = xdb_file_full(flag_set, dp->p, xf->spool, 
							 dp->id->server, "global", "xdb");
    }

    if(full == NULL) {
	  /* return data if get */
	  if (flag_set == 0) {
		xmlnode_put_attrib(dp->x,"type","result");
		xmlnode_put_attrib(dp->x,"to",xmlnode_get_attrib(dp->x,"from"));
		xmlnode_put_attrib(dp->x,"from",jid_full(dp->id));
		deliver(dpacket_new(dp->x), NULL);
		return r_DONE;
	  }

	  return r_ERR;
	}
	
	log_debug(ZONE,"%s",full);

    /* =================================  S E T  ===================================*/
    if (flag_set == 1)  {
	  char *act,*match;
	  int ret = 1;

	  SEM_LOCK(xf->hash_sem);
	  if((c = wpxhash_get(xf->hash,full)) != NULL)  {
		/* if no data */
		if (c->file == NULL) {
		  c->ref++;
		  
		  /* wait for data, I don't like this :( */
		  SEM_UNLOCK(xf->hash_sem);
		  
		  while (1) {
			usleep(15);
			SEM_LOCK(xf->hash_sem);
			if (c->file != NULL) break;
			SEM_UNLOCK(xf->hash_sem);
		  }
		}
		else
		  c->ref++;

		/* data take */
		top = c->file;

		/* set data are busy */
		c->file = NULL;
		
		/* hash free */
		SEM_UNLOCK(xf->hash_sem);
	  }
	  else 	{	
		pool p = pool_heap(1024);
		c = pmalloco(p,sizeof(_cacher));
		c->ref = 1;

		wpxhash_put(xf->hash,pstrdup(p,full),c);

		/* unlock hash */	   
		SEM_UNLOCK(xf->hash_sem);
		
		/* parse file */
		top = xdb_file_parse(c->wpkey,p);
		  
		if(top == NULL)
		  top = xmlnode_new_tag_pool(p,"xdb");		
	  }

	  // ++ top has data and c->file = NULL and sem if unlocked here 
	  file = top;
	  
	  /* if we're dealing w/ a resource, just get that element */		
	  if(dp->id->resource != NULL) {
		if (
			(top = xmlnode_get_tag(top,spools(dp->p,"res?id=",dp->id->resource,dp->p))) == NULL
			) {
		  top = xmlnode_insert_tag(file,"res");
		  xmlnode_put_attrib(top,"id",dp->id->resource);
		}
	  }

	  /* data from xdb */
	  data = xmlnode_get_tag(top,spools(dp->p,"?xdbns=",ns,dp->p));
	  
	  act = xmlnode_get_attrib(dp->x,"action");
	  match = xmlnode_get_attrib(dp->x,"match");
	  
	  if (act != NULL)    {
		switch(*act)
		  {
		  case 'i':
			/* we're inserting into something that doesn't exist?!?!? */
			if(data == NULL) { 
			  data = xmlnode_insert_tag(top,"foo");
			  xmlnode_put_attrib(data,"xdbns",ns);
			  xmlnode_put_attrib(data,"xmlns",ns); /* should have a top-level xmlns attrib */
			}
			xmlnode_hide(xmlnode_get_tag(data,match)); /* any match is a goner */
			
			xmlnode_insert_tag_node(data, xmlnode_get_firstchild(dp->x));		    
			break;

		  case 'c': /* check action */
			if(match != NULL)
			  data = xmlnode_get_tag(data,match);
			if(j_strcmp(xmlnode_get_data(data),
						xmlnode_get_data(xmlnode_get_firstchild(dp->x))) != 0) {
			  log_debug(ZONE,"xdb check action returning error to signify unsuccessful check");
			  ret = 0;
			}
			flag_set = 0;
			break;

		  default:
			log_warn("xdb_file","unable to handle unknown xdb action '%s'",act);
			ret = 0;
			break;
		  }
	  }
	  else {
		/* overwrite */
		if(data != NULL)
		  xmlnode_hide(data);
		
		/* copy the new data into file */
		data = xmlnode_insert_tag_node(top, xmlnode_get_firstchild(dp->x));
		xmlnode_put_attrib(data,"xdbns",ns);
	  }
	  	
	  /* if ready to send result */
	  if (ret)  {	    	
		
		if (flag_set) {
		  int result = xdb_file2file(c->wpkey,file,xf->sizelimit);
		  
		  if (result < 0) {
			log_error(dp->id->server,"xdb request failed, unable to save to file %s",
					  c->wpkey);
			ret = 0;
		  }
		  else if (result == 0) {
			/* size reached */
			ret = 0;
		  }
		}
		
		if (ret) {			    
		  xmlnode_put_attrib(dp->x,"type","result");
		  xmlnode_put_attrib(dp->x,"to",xmlnode_get_attrib(dp->x,"from"));
		  xmlnode_put_attrib(dp->x,"from",jid_full(dp->id));
		  deliver(dpacket_new(dp->x), NULL); 
		}
	  }
	  
	  SEM_LOCK(xf->hash_sem);
	  
	  /* our ref-- */
	  c->ref--;
	  
	  /* if nobody waits data always remove */
	  if (c->ref == 0) {
		wpxhash_zap(xf->hash,c->wpkey);
		remove = 1;
		if (xf->timeout > 0) {
		  cacher_remove_element(xf,c);
		}
	  }
	  else {
		/* if someone waits data, give data */
		c->file = file;
	  }
	  
	  SEM_UNLOCK(xf->hash_sem);
	  
	  if (remove)
		xmlnode_free(file);
	  
	  if(ret)
		return r_DONE;
	  else
		return r_ERR;
	}
    /* =================================  G E T  ===================================*/   
    else {
	  /* 
         If element exist get data or wait for them
		 If no element, create element, get data, update element 
	  */		       
	  SEM_LOCK(xf->hash_sem);
	  if((c = wpxhash_get(xf->hash,full)) != NULL)  {		
		/* if no data */
		if (c->file == NULL) {
		  c->ref++;
		  
		  /* wait data, I don't like this :( */
		  SEM_UNLOCK(xf->hash_sem);
		  
		  while (1) {
			usleep(15);
			
			SEM_LOCK(xf->hash_sem);
			if (c->file != NULL) break;
			SEM_UNLOCK(xf->hash_sem);
		  }		
  
		  c->ref--;

		  /* if no cache and nobody waits data , remove */
		  if ((xf->timeout == 0)&&(c->ref==0)) {
			wpxhash_zap(xf->hash,c->wpkey);
			remove = 1;
		  }
		  else {
			if (xf->timeout > 0) {
			  c->lasttime = xf->actualtime;
			  cacher_touch_element(xf,c);
			}
		  }
		}
		else {
		  if (xf->timeout > 0) {
			c->lasttime = xf->actualtime;
			cacher_touch_element(xf,c);
		  }
		}

		/* has data */
		top = c->file;
	  }
	  else 	{	
		pool p = pool_heap(1024);
		c = pmalloco(p,sizeof(_cacher));
		c->ref = 1;

		wpxhash_put(xf->hash,pstrdup(p,full),c);

		/* unlock hash */	   
		SEM_UNLOCK(xf->hash_sem);
		
		log_debug(ZONE,"get new %s",c->wpkey);		
		
		/* parse file */
		top = xdb_file_parse(c->wpkey,p);
		  
		if(top == NULL)
		  top = xmlnode_new_tag_pool(p,"xdb");		
		
		log_debug(ZONE,"parse: %s",xmlnode2str(top));
		
		/* lock */
		SEM_LOCK(xf->hash_sem);

		/* ref-- */
		c->ref--;

		/* if no cache and nobody waits data , remove */
		if ((xf->timeout == 0)&&(c->ref==0)) {
		  /* remove from hash */		  
		  wpxhash_zap(xf->hash,c->wpkey);
		  remove = 1;
		}
		else {
		  c->file = top;
		  if (xf->timeout > 0) {
			c->lasttime = xf->actualtime;
			cacher_touch_element(xf,c);
		  }
		}
	  }
	  // ++ top has data and sem hash is locked here

	  file = top;

	  /* if we're dealing w/ a resource, just get that element */		
	  if(dp->id->resource != NULL) {
		if (
			(top = xmlnode_get_tag(top,spools(dp->p,"res?id=",dp->id->resource,dp->p))) == NULL) {
		  top = xmlnode_insert_tag(file,"res");
		  xmlnode_put_attrib(top,"id",dp->id->resource);
		}
	  }
	  
	  /* just query the relevant namespace */
	  data = xmlnode_get_tag(top,spools(dp->p,"?xdbns=",ns,dp->p));
	  
	  if(data != NULL) { /* cool, send em back a copy of the data */
		xmlnode_hide_attrib(xmlnode_insert_tag_node(dp->x, data),"xdbns");
	  }

	  /* unlock hash */
	  SEM_UNLOCK(xf->hash_sem);
	  	  
	  xmlnode_put_attrib(dp->x,"type","result");
	  xmlnode_put_attrib(dp->x,"to",xmlnode_get_attrib(dp->x,"from"));
	  xmlnode_put_attrib(dp->x,"from",jid_full(dp->id));
	  deliver(dpacket_new(dp->x), NULL);
	  
	  if (remove)
		xmlnode_free(file);	

	  return r_DONE;
    }
}

void xdb_file_cleanup(void *arg)
{
  xdbf xf = (xdbf)arg;

  SEM_LOCK(xf->hash_sem);     
  xf->shutdown = 1;
  /* hash walk and free all elements, but why ?? */
  SEM_UNLOCK(xf->hash_sem);
}

void xdb_file(instance i, xmlnode x)
{
    char *spl,*to;
    xmlnode config;
    xdbcache xc;
    xdbf xf;
    int timeout = 0; /* defaults to timeout forever */
    int sizelimit = 0; /* no size limit */

    log_debug(ZONE,"xdb_file loading");

    xc = xdb_cache(i);
    config = xdb_get(xc, jid_new(xmlnode_pool(x),"config@-internal"),"jabber:config:xdb_file");

    spl = xmlnode_get_tag_data(config,"spool");
    if(spl == NULL)
    {
        log_error(NULL,"xdb_file: No filesystem spool location configured");
        return;
    }
    to = xmlnode_get_tag_data(config,"timeout");
    if(to != NULL)
        timeout = atoi(to);

    to = xmlnode_get_tag_data(config,"sizelimit");
    if(to != NULL)
        sizelimit = atoi(to);

    xf = pmalloco(i->p,sizeof(_xdbf));
    xf->spool = pstrdup(i->p,spl);
	xf->shutdown = 0;
    xf->timeout = timeout;
    xf->sizelimit = sizelimit;
    xf->i = i;
    xf->actualtime = time(NULL);
	xf->last = NULL;  //last element in list, start removing from here
	xf->first = NULL; //first element in list

    SEM_INIT(xf->hash_sem);
	
    xf->hash = wpxhash_new(j_atoi(xmlnode_get_tag_data(config,"maxfiles"),FILES_PRIME));
	
    log_debug(ZONE,"Starting XDB size limit =  %d, timeout = %d",sizelimit,timeout);
	
    register_phandler(i, o_DELIVER, xdb_file_phandler, (void *)xf);

    if(timeout > 0) /* 0 is expired immediately, -1 is cached forever */
       register_beat(10, xdb_file_purge, (void *)xf); 

    xmlnode_free(config);
    pool_cleanup(i->p, xdb_file_cleanup, (void*)xf);
}
