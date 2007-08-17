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

/* ---------------------------------------------------------
   base_connect - Connects to a specified host/port and 
                  exchanges xmlnodes with it over a socket
   ---------------------------------------------------------

   USAGE:
     <connect>
        <ip>1.2.3.4</ip>
	    <port>2020</port>
	    <secret>foobar</secret>
	    <timeout>5</timeout>
        <tries>15</tries>
     </connect>

   TODO: 
   - Add packet aging/delivery heartbeat
*/

/* Connection states */
typedef enum { conn_DIE, conn_CLOSED, conn_OPEN, conn_AUTHD } conn_state;

typedef struct queue_struct
{
  //    int stamp;
    xmlnode x;
    struct queue_struct *next;
} *queue, _queue;


/* conn_info - stores thread data for a connection */
typedef struct
{
    mio           io;
    conn_state    state;	/* Connection status (closed, opened, auth'd) */
    char*         hostip;	/* Server IP */
    u_short       hostport;	/* Server port */
    char*         secret;	/* Connection secret */
    char*         name;		/* Name we're connecting under */
    int           timeout;  /* timeout for connect() in seconds */
    int           tries_left;   /* how many more times are we going to try to connect? */
    pool          mempool;	/* Memory pool for this struct */
    instance      inst;     /* Matching instance for this connection */
    char * event;           /* event, informs when connection is opened, closed */
    pthread_mutex_t sem;
    queue q_start,q_end;
    dpacket dplast; /* special circular reference detector */
} *conn_info, _conn_info;

/* conn_write_buf - stores a dpacket that needs to be written to the socket */
//typedef struct
//{
  //    pth_message_t head;
//    dpacket     packet;
//} *conn_write_buf, _conn_write_buf;



void base_connect_process_xml(mio m, int state, void* arg, xmlnode x);


void base_connect_queue(conn_info ci, xmlnode x)
{
    queue q;
    if(ci == NULL || x == NULL) return;

    log_debug(ZONE,"insert packet into queue");

    pthread_mutex_lock(&(ci->sem));
    q = pmalloco(xmlnode_pool(x),sizeof(_queue));
    q->x = x;
    q->next = NULL;
    
    if (ci->q_end)
      ci->q_end->next = q;
    else
      ci->q_start = q;

    ci->q_end = q;
    pthread_mutex_unlock(&(ci->sem));
}


/* Deliver packets to the socket io thread */
result base_connect_deliver(instance i, dpacket p, void* arg)
{
    conn_info ci = (conn_info)arg;

    /* Insert the message into the write_queue if we don't have an MIO socket yet.. */
    if ((ci->state != conn_AUTHD)&&(ci->state != conn_DIE))
    {
        base_connect_queue(ci, p->x);
    }
    /* Otherwise, write directly to the MIO socket */
    else
    {
        if(ci->dplast == p) /* don't handle packets that we generated! doh! */
            deliver_fail(p,"Circular Reference Detected");
        else
            mio_write(ci->io, p->x, NULL, 0);
    }

    return r_DONE;

}

/* this runs from another thread under mtq */
void base_connect_connect(void *arg)
{
    conn_info ci = (conn_info)arg;
    sleep(2); /* take a break */
    mio_connect(ci->hostip, ci->hostport, base_connect_process_xml, (void*)ci, ci->timeout, NULL, mio_handlers_new(NULL, NULL, MIO_XML_PARSER));
}

void base_connect_process_xml(mio m, int state, void* arg, xmlnode x)
{
    conn_info ci = (conn_info)arg;
    xmlnode cur;
    queue q,q2;
    char  hashbuf[41];

    log_debug(ZONE, "process XML: m:%X state:%d, arg:%X, x:%X", m, state, arg, x);

    switch (state)
    {
        case MIO_NEW:

            ci->state = conn_OPEN;
            ci->io = m;

            /* Send a stream header to the server */
            log_debug(ZONE, "base_connecting: %X, %X, %s", ci, ci->inst, ci->inst->id); 

            cur = xstream_header("jabber:component:accept", ci->name?ci->name:ci->inst->id, NULL);
            mio_write(m, NULL, xstream_header_char(cur), -1);
            xmlnode_free(cur);

            return;

        case MIO_XML_ROOT:
            /* Extract stream ID and generate a key to hash */
            shahash_r(spools(x->p, xmlnode_get_attrib(x, "id"), ci->secret, x->p), hashbuf);

            /* Build a handshake packet */
            cur = xmlnode_new_tag("handshake");
            xmlnode_insert_cdata(cur, hashbuf, -1);

            /* Transmit handshake */
            mio_write(m, NULL, xmlnode2str(cur), -1);

            xmlnode_free(cur);
            xmlnode_free(x);
            return;

        case MIO_XML_NODE:
            /* Only deliver packets after the connection is auth'd */
            if (ci->state == conn_AUTHD)
            {
	      if (j_strcmp(xmlnode_get_name(x), "endconnection") == 0){
			log_alert(ZONE,"Server is closing. stop sending to him");
			xmlnode_free(x);
			ci->state = conn_CLOSED;

			if (ci->event != NULL) {
			  xmlnode cur;

			  log_debug(ZONE,"event disconnection");

			  cur = xmlnode_new_tag("route");
			  xmlnode_put_attrib(cur,"type","disconnected");
			  xmlnode_put_attrib(cur,"to",ci->event);
			  xmlnode_put_attrib(cur,"from",ci->inst->id);
			  deliver(dpacket_new(cur),ci->inst);
			}


			return;
	      }
	      else {
                ci->dplast = dpacket_new(x); /* store the addr of the dpacket we're sending to detect circular delevieries */
                deliver(ci->dplast, ci->inst);
                ci->dplast = NULL;
                return;
	      }
            }

            /* If a handshake packet is recv'd from the server, we have successfully auth'd -- go ahead and update the connection state */
            if (j_strcmp(xmlnode_get_name(x), "handshake") == 0)
            {
                /* Flush all packets queued up for delivery */
	      log_debug(ZONE,"flush old packets");
	      pthread_mutex_lock(&(ci->sem));
	      q = ci->q_start;
	      while(q != NULL)
	      {
                q2 = q->next;
                mio_write(ci->io, q->x, NULL, 0);
                q = q2;
	      }
	      ci->q_start = NULL;
	      ci->q_end = NULL;
	      pthread_mutex_unlock(&(ci->sem));

	      ci->state = conn_AUTHD;
            }
            xmlnode_free(x);
			
			if (ci->event != NULL) {
			  xmlnode cur;

			  log_debug(ZONE,"event connection");

			  cur = xmlnode_new_tag("route");
			  xmlnode_put_attrib(cur,"type","connected");
			  xmlnode_put_attrib(cur,"to",ci->event);
			  xmlnode_put_attrib(cur,"from",ci->inst->id);
			  deliver(dpacket_new(cur),ci->inst);
			}

            return;

        case MIO_CLOSED:

			if ((ci->state == conn_AUTHD)&&(ci->event != NULL)) {
			  xmlnode cur;

			  log_debug(ZONE,"event disconnection");

			  cur = xmlnode_new_tag("route");
			  xmlnode_put_attrib(cur,"type","disconnected");
			  xmlnode_put_attrib(cur,"to",ci->event);
			  xmlnode_put_attrib(cur,"from",ci->inst->id);
			  deliver(dpacket_new(cur),ci->inst);
			}

            if(ci->state == conn_DIE)
                return; /* server is dying */

            /* Always restart the connection after it closes for any reason */
            ci->state = conn_CLOSED;
            if(ci->tries_left != -1) 
                ci->tries_left--;

            if(ci->tries_left == 0)
            {
                log_alert(ZONE, "Base Connect Failed: service %s was unable to connect to %s:%d, unrecoverable error, exiting", ci->inst->id, ci->hostip, ci->hostport);
		//                fprintf(stderr, "Base Connect Failed: service %s was unable to connect to %s:%d, unrecoverable error, exiting", ci->inst->id, ci->hostip, ci->hostport);
		/* it shoudnt happen */
		//                exit(1);
            }

            /* pause 2 seconds, and reconnect */
            log_debug(ZONE, "Base Connect Failed to connect to %s:%d Retry [%d] in 2 seconds...", ci->hostip, ci->hostport, ci->tries_left);
            mtq_send(NULL,ci->mempool,base_connect_connect,(void *)ci);

            return;
    }
}

void base_connect_kill(void *arg)
{
    conn_info ci = (conn_info)arg;
    ci->state = conn_DIE;
}

result base_connect_config(instance id, xmlnode x, void *arg)
{
    char*     secret = NULL;
    char*     name = NULL;
    int       timeout;
    int       tries;
    int port;
    conn_info ci = NULL;

    /* Extract info */
    port    = j_atoi(xmlnode_get_tag_data(x, "port"),0);
    secret  = xmlnode_get_tag_data(x, "secret");
    name    = xmlnode_get_tag_data(x, "name");
    timeout = j_atoi(xmlnode_get_tag_data(x, "timeout"), 5);
    tries   = j_atoi(xmlnode_get_tag_data(x, "tries"), -1); 

    if(id == NULL)
    {
        log_debug(ZONE,"base_accept_config validating configuration\n");
        if(port == 0 || (secret == NULL))
        {
            xmlnode_put_attrib(x, "error", "<connect> requires the following subtags: <port>, and <secret>");
            return r_ERR;
        }
        return r_PASS;
    }

    log_debug(ZONE, "Activating configuration: %s\n", xmlnode2str(x));

    /* Allocate a conn structures, using this instances' mempool */
    ci              = pmalloco(id->p, sizeof(_conn_info));
    ci->mempool     = id->p;
    ci->state       = conn_CLOSED;
    ci->inst        = id;
    ci->hostip      = pstrdup(ci->mempool, xmlnode_get_tag_data(x,"ip"));
    if(ci->hostip == NULL) ci->hostip = pstrdup(ci->mempool, "127.0.0.1");
    ci->hostport    = port;
    ci->secret      = pstrdup(ci->mempool, secret);
    ci->name        = name?pstrdup(ci->mempool, name):NULL;
    ci->event       = xmlnode_get_tag_data(x, "event");
    pthread_mutex_init(&(ci->sem),NULL);    /* sem for queue */
    //    ci->write_queue = pth_msgport_create(ci->hostip);

    ci->timeout     = timeout;
    ci->tries_left  = tries;

    /* Register a handler to recieve inbound data for this instance */
    register_phandler(id, o_DELIVER, base_connect_deliver, (void*)ci);
     
    /* Make a connection to the host, in another thread */
    mtq_send(NULL,ci->mempool,base_connect_connect,(void *)ci);

    register_shutdown(base_connect_kill, (void *)ci);

    return r_DONE;
}

void base_connect(void)
{
    log_debug(ZONE,"base_connect loading...\n");
    register_config("connect",base_connect_config,NULL);
}

