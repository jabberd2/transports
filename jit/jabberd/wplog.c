/* --------------------------------------------------------------------------
    WP log module
    Author: £ukasz Karwacki.

    Module logs number of messages, iq, presences for stats.
 * --------------------------------------------------------------------------*/
#include "jabberd.h"

wplog __wplog = NULL;

extern xmlnode greymatter__;

result wplog_beat(void *arg)
{
  FILE * plik;
  char date[50],buf[200];
  struct tm *today;
  unsigned long ltime;
  unsigned int msg,pres,query,files;
  unsigned int msg1,pres1,query1,files1;
  wplog_packet wplogp = (wplog_packet)arg;
  
  msg    = wplogp->messages;
  pres   = wplogp->presences;
  query  = wplogp->queries;
  files  = wplogp->files;

  msg1   = wplogp->last_messages;
  pres1  = wplogp->last_presences;
  query1 = wplogp->last_queries;
  files1 = wplogp->last_files;

  /* only here last are changed */
  wplogp->last_messages  = msg;
  wplogp->last_presences = pres;
  wplogp->last_queries   = query;
  wplogp->last_files   = files;

  msg   -= msg1;
  pres  -= pres1;
  query -= query1;
  files -= files1;

  if (msg < 0) msg = -msg;
  if (pres < 0) pres = -pres;
  if (query < 0) query = -query;
  if (files < 0) files = -files;

  time( &ltime );
  today = localtime( &ltime );
  strftime((char *)date,128,"%Y_%m_%d",today);
  sprintf(buf,"%s_%s.log",wplogp->file,date);    

  plik = fopen(buf, "at");
  if (plik) {
    fprintf(plik, "%s m:\t%u\tp:\t%u\tq:\t%u\tf:\t%u\r\n", 
	    jutil_timestamplocal(),msg,pres,query,files);

    fclose(plik);
  }

  return r_DONE;
}


void wplog_init(void)
{
    struct tm *today;
    unsigned long ltime;

    pool p;
    int i;
    wplog_packet wplogp;
    xmlnode cur;
    xmlnode wplogcfg = xmlnode_get_tag(greymatter__, "wp");

    p             = pool_new();
    __wplog       = pmalloco(p, sizeof(_wplog));
    __wplog->p    = p;
    wplogp        = pmalloco(p, sizeof(_wplog_packet)*10);
    __wplog->server_start = pmalloco(p, 30);

    /* server start time */
    time( &ltime );
    today = localtime( &ltime );
    strftime((char *)__wplog->server_start,28,"%Y%m%d%H%M%S",today);

    for (i=0; i < MAX_WPLOGS; i++){
      __wplog->tab[i] = wplogp + i;
      __wplog->tab[i]->active = 0;
      __wplog->tab[i]->time   = 1;
      __wplog->tab[i]->file   = NULL;
    }

    /* read configuration */
    for(cur = xmlnode_get_firstchild(wplogcfg); cur != NULL; cur = xmlnode_get_nextsibling(cur)) {
      char * file;
      int number;
      int time;
      
      if (xmlnode_get_type(cur) != NTYPE_TAG) continue;
      
      if ((file = xmlnode_get_attrib(cur,"file")) == NULL)  continue;
      number = j_atoi(xmlnode_get_attrib(cur, "number"), -1);
      if ((number < 0) || (number >= MAX_WPLOGS)) continue;

      time = j_atoi(xmlnode_get_attrib(cur, "time"), 60);
      if (time < 1)  continue;

      /* try to add log */
      /* if already in use */
      if (__wplog->tab[number]->active){
	fprintf(stderr,"Error in wplog configuration. Number already in use.");
      }

      __wplog->tab[number]->active = 1;
      __wplog->tab[number]->file   = pstrdup(p,file);
      __wplog->tab[number]->time   = time;
    }

    __wplog->logs = -1;

    for (i=0; i < MAX_WPLOGS; i++){
      if (__wplog->tab[i]->active){
		/* max wplog */
		__wplog->logs = i;
		
		/* init log */
		log_debug(ZONE,"Start wplog %s %d",
				  (__wplog->tab[i]->file), 
				  (__wplog->tab[i]->time));
		
		register_beat(__wplog->tab[i]->time, wplog_beat, (void *)__wplog->tab[i]);
      }
    }
}


