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

typedef struct basefile_struct {
  char * filename;
  struct tm yesterday;
  unsigned long last_time;
  pthread_mutex_t sem;
  FILE * f;
} *basefile, _basefile;

result base_file_deliver(instance id, dpacket p, void* arg)
{
  char* message = NULL;
  basefile bf = (basefile) arg;
  char date[50],buf[200];
  struct tm *today;
  unsigned long ltime;
  FILE * f;

  time( &ltime );

  /* once per second */
  if ((ltime > bf->last_time) || (bf->f == NULL)) {

    /* try to route */
    bf->last_time = ltime;

    /* lock */
    pthread_mutex_lock(&(bf->sem));

    today = localtime( &ltime );
    /* if day changed or new raport */
    if ((bf->yesterday.tm_mday != today->tm_mday) || (bf->f == NULL)) {
      memcpy(&(bf->yesterday),today,sizeof(struct tm));
      strftime((char *)date,128,"%Y_%m_%d",today);
      sprintf(buf,"%s_%s.log",bf->filename,date);    
      
      f = bf->f;
      bf->f = fopen(buf,"at");
      if (f)
	fclose(f);
    }

    /* unlock */
    pthread_mutex_unlock(&(bf->sem));
  }
  
  if (bf->f == NULL)
    {
      log_debug(ZONE,"base_file_deliver error: no file available to print to.\n");
      return r_ERR;
    }
  
  message = xmlnode_get_data(p->x);
  if (message == NULL) {
      log_debug(ZONE,"base_file_deliver error: no message available to print.\n");
      return r_ERR;
  }
  
  if (fprintf(bf->f,"%s\n", message) == EOF) {
        log_debug(ZONE,"base_file_deliver error: error writing to file(%d).\n", errno);
        return r_ERR;
  }
  fflush(bf->f);

  /* Release the packet */
  pool_free(p->p);
  return r_DONE;    
}

void _base_file_shutdown(void *arg)
{
  basefile bf = (basefile) arg;
  FILE *f;
    
  f = bf->f;
  bf->f = NULL;
  if (f)
    fclose(f);
}

result base_file_config(instance id, xmlnode x, void *arg)
{
    basefile bf;
        
    if(id == NULL)
    {
        log_debug(ZONE,"base_file_config validating configuration");

        if (xmlnode_get_data(x) == NULL)
        {
            log_debug(ZONE,"base_file_config error: no filename provided.");
            xmlnode_put_attrib(x,"error","'file' tag must contain a filename to write to");
            return r_ERR;
        }
        return r_PASS;
    }

    log_debug(ZONE,"base_file configuring instance %s",id->id);

    if(id->type != p_LOG)
    {
        log_alert(NULL,"ERROR in instance %s: <file>..</file> element only allowed in log sections", id->id);
        return r_ERR;
    }

    bf = pmalloco(id->p,sizeof(_basefile));
    bf->filename = pstrdup(id->p,xmlnode_get_data(x));
    bf->yesterday.tm_mday = -1;
    bf->last_time = 0;
    bf->f = NULL;
    pthread_mutex_init(&(bf->sem),NULL);

    /* Register a handler for this instance... */
    register_phandler(id, o_DELIVER, base_file_deliver, (void*)bf);

    pool_cleanup(id->p, _base_file_shutdown, (void*)bf);
    
    return r_DONE;
}

void base_file(void)
{
    log_debug(ZONE,"base_file loading...");
    register_config("file",base_file_config,NULL);
}


