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
#define NODEBUG

#include "jabberd.h"

/*** mtq is Managed Thread Queues ***/
/* they queue calls to be run sequentially on a thread, that comes from a system pool of threads */


mtqmaster mtq__master = NULL;
volatile int mtq__shutdown = 0;

void mtq_cleanup(void *arg)
{
    mtq q = (mtq)arg;
    
    q->users_count--;
}

void mtq_init();

/* public queue creation function, queue lives as long as the pool */
mtq mtq_new_que(pool p,  mtq que)
{
    mtq q;
    int n,mtq_n;
    int count;

    log_debug(ZONE,"MTQ(new)");


	if (p==NULL)
	  return NULL;

    if(mtq__master == NULL)  {
	  mtq_init();
	}
	
	if (que) {
	  que->users_count++;
	  pool_cleanup(p, mtq_cleanup, (void *)que);
	  return que;
	}

	/* find queue with less users */
	count=999999;
	mtq_n = 0;
	for(n = 0; n < MTQ_THREADS; n++)
	  if(mtq__master->all[n]->mtq->users_count < count)  {	    
		count = mtq__master->all[n]->mtq->users_count;
		mtq_n = n;
	  }
	q = mtq__master->all[mtq_n]->mtq;    

    q->users_count++;    
    pool_cleanup(p, mtq_cleanup, (void *)q);

    return q;
}

/* public queue creation function, queue lives as long as the pool */
mtq mtq_new(pool p)
{
    mtq q;
    int n,mtq_n;
    int count;

	if (p==NULL)
	  return NULL;

    if(mtq__master == NULL)  {
	  mtq_init();
	}

	/* find queue with less users */
	count=999999;
	mtq_n = 0;
	for(n = 0; n < MTQ_THREADS; n++)
	  if(mtq__master->all[n]->mtq->users_count < count)  {	    
		count = mtq__master->all[n]->mtq->users_count;
		mtq_n = n;
	  }
	q = mtq__master->all[mtq_n]->mtq;    

    q->users_count++;    
    pool_cleanup(p, mtq_cleanup, (void *)q);

    return q;
}

/* main slave thread */
void *mtq_main(void *arg)
{
    mth t = (mth)arg;
    mtqqueue mq; /* temp call structure */
    _mtqqueue mqcall; /* temp call structure */
    log_debug("mtq","%X starting mq=%d",t->thread,t->mtq);

    while(1) {

	  /* check if something to resolv */
	  SEM_LOCK(t->mtq->sem);
	  if (t->mtq->last == NULL) {
		COND_WAIT(t->mtq->cond,t->mtq->sem);
	  }

	  /* get element */
	  mq = t->mtq->last;
	  if (mq != NULL) {
		mqcall.f = mq->f;
		mqcall.arg = mq->arg;
		
		/* remove call from list */		    
		t->mtq->last = t->mtq->last->prev;		    
		t->mtq->dl--;
		
		/* add mq to list */
		mq->prev = NULL;
		
		if (t->mtq->free_last == NULL)
		  t->mtq->free_last = mq;
		else
		  t->mtq->free_first->prev = mq;
		
		t->mtq->free_first = mq;		
	  }

	  SEM_UNLOCK(t->mtq->sem);

	  if (mq != NULL) {
		(*(mqcall.f))(mqcall.arg);
	  }
	  else {	  
		/* mq is NULL here, so no more packets in queue */
		if (mtq__shutdown == 1)
		  break;		
	  }
	} /* loop end */
    
    log_debug("mtq","%X ending mq=%d",t->thread,t->mtq);
    return NULL;
}

void mtq_stop()
{
    mth t = NULL;
    int n;
    int l,m; /* counter for mtq long */
    void *ret;

    if ( mtq__master == NULL )
       return;

    /* 3 times check all quees */
    m = 3;
    while (m > 0) {

      l = 0;      

      for(n=0;n < MTQ_THREADS;n++) {
        t = mtq__master->all[n];	
		l += t->mtq->dl;
      }
      
      if (l == 0) 
		m--;
      else 
		if (m < 3) m = 3;
    }
    
    /* set exit flag */
    mtq__shutdown = 1;

    /* wait exit */
    for(n=0;n < MTQ_THREADS;n++)
    {
        t = mtq__master->all[n];
		/* signal exit */
		COND_SIGNAL(t->mtq->cond);
        /* wait */
		pthread_join(t->thread,&ret);
    }

    /* clean MTQ */
    for(n=0;n<MTQ_THREADS;n++)
    {
        t = mtq__master->all[n];
      	pthread_mutex_destroy(&(t->mtq->sem));      
        pool_free(t->p);
    }

    ret = mtq__master;
    mtq__master = NULL;
    free(ret);
}

void mtq_send(mtq q, pool p, mtq_callback f, void *arg)
{
    mtqqueue mq;  /* one element */ 
	mtq mtq;


    /* initialization stuff */
    if(mtq__master == NULL)
    {
	  mtq_init();
    }

    if(q != NULL)  {
      mtq = q;
    }
    else {
      /* take next thread */
	  mtq__master->random++;
	  if (mtq__master->random >= MTQ_THREADS)
		mtq__master->random = 0;
	  
	  mtq = mtq__master->all[mtq__master->random]->mtq;      
    }
	
    /* build queue */
    log_debug(ZONE,"mtq_send insert into mtq=%p",mtq);

    /* lock operation on queue */
    SEM_LOCK(mtq->sem);

    /* find free memory */
    mq = mtq->free_last;
            
    if (mq == NULL)
    {  
	  while ((mq = malloc(sizeof(_mtqqueue)))==NULL) Sleep(1000);
	  /* it means new malloc 
		 maybe we should free this mq later ? */
	  log_alert(ZONE,"MTQ new queue malloc");
	  mq->memory = 1;
	  mtq->length++;
    }
    else   {
	  /* take it out from queue */
	  mtq->free_last = mtq->free_last->prev;
    }
    
    mq->f = f;
    mq->arg = arg;
    mq->prev = NULL;
    
    mtq->dl++;
    
    /* if queue is empty */
    if (mtq->last == NULL)
      mtq->last = mq;
    else
      mtq->first->prev = mq;

    mtq->first = mq;

	COND_SIGNAL(mtq->cond);

    SEM_UNLOCK(mtq->sem);
}

void mtq_init() {
    mtq mtq = NULL; /* queue */
    mth t = NULL;
    int n,k; 
    pool newp;


	mtq__master = malloc(sizeof(_mtqmaster)); /* happens once, global */
	mtq__master->random = 0;

	/* start MTQ threads */
	for(n=0;n<MTQ_THREADS;n++)  {
	  newp = pool_new();
	  t = pmalloco(newp, sizeof(_mth));
	  t->p = newp;
	  t->mtq = pmalloco(newp,sizeof(_mtq));
	  t->mtq->first = t->mtq->last = NULL;
	  t->mtq->free_first = t->mtq->free_last = NULL;	    
	  t->mtq->users_count = 0;
	  t->mtq->dl = 0;	    
	  t->mtq->length = 0;

	  mtq = t->mtq;
	
	  /* build queue cache */
	  for (k=0;k<MTQ_QUEUE_LONG;k++)  {
		/* mtq->free_last if the first to take from queue*/	
		mtq->queue[k].memory = 0;
		mtq->queue[k].prev   = NULL;
		
		/* if queue is empty */
		if (mtq->free_last == NULL)
		  mtq->free_last = &(mtq->queue[k]);
		else
		  mtq->free_first->prev = &(mtq->queue[k]);
		
		mtq->free_first = &(mtq->queue[k]);
		mtq->length++;
	  }
	  
	  SEM_INIT(t->mtq->sem);
	  COND_INIT(t->mtq->cond);
		
	  pthread_create(&(t->thread), NULL, mtq_main, (void *)t);
	  mtq__master->all[n] = t; /* assign it as available */
	}
}







