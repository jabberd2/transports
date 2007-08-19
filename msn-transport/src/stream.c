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
 * Copyright (c) 2000-2001 Schuyler Heath <sheath@jabber.org>
 *
 * Acknowledgements
 *
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 *
 * -------------------------------------------------------------------------- */

#include "stream.h"
#include "utils.h"

static char *scratch;
static int scratch_sz;

#define SCRATCH_INC 100
#define PACKET_ADD(p,c,v) p = mt_realloc(p,sizeof(char *) * (c + 1)); p[c++] = v

void mt_stream_free(mpstream st)
{
    mphandler cur = st->head, tmp;

    log_debug(ZONE,"freeing stream %X",st);

    (st->cb)(NULL,st->arg);

    while (cur != NULL)
    {
        tmp = cur;
        cur = cur->next;
        mt_free(tmp);
    }

    if (st->buffer)
        mt_free(st->buffer);

    if (st->mp)
    {
        mt_free(st->mp->params);
        pool_free(st->mp->p);
    }

    mt_free(st);
}

char *mt_packet2str(mpacket mp)
{
    int i;
    spool sp;

    sp = spool_new(mp->p);
    for (i = 0; i < mp->count; i++)
    {
        spool_add(sp,mp->params[i]);
        if (i + 1 < mp->count)
            spool_add(sp," ");
    }

    return spool_print(sp);
}

void mt_stream_packet(mpstream st, mpacket mp)
{
    unsigned long trid;

#ifdef PACKET_DEBUG
    log_debug(ZONE,"PACKET DUMP:\n%s",mt_packet2str(mp));
#endif

    trid = atol(mt_packet_data(mp,1));
    if (trid != 0)
    {
        mphandler cur = st->head, prev = NULL;

        while (st->closed == 0 && cur != NULL)
        {
            /* look for a packet registered handler */
            if (cur->trid != trid)
            {
                prev = cur;
                cur = cur->next;
                continue;
            }

            log_debug(ZONE,"Packet handler found");

            switch ((cur->cb)(mp,cur->arg))
            {
            case r_ERR:
                log_error(NULL,"Error processing packet! %s",mt_packet2str(mp));
            case r_DONE:
                if (prev == NULL)
                    st->head = cur->next;
                else if ((prev->next = cur->next) == NULL)
                    st->tail = prev;
                mt_free(cur);
                break;

            default: /* r_PASS is the only other result returned */
                break;
            }

            log_debug(ZONE,"Packet handled");
            goto done;
        }
    }

    /* there was no registered handler for this packet, or the stream is closing */
    if ((st->cb)(mp,st->arg) == r_ERR)
    {
        log_debug(ZONE,"Default packet handler failed!");
    }

done:
    mt_free(mp->params);
    pool_free(mp->p);
}


int mt_stream_parse_msg(mpacket mp, int msg_len, char *buffer, int sz)
{
    char *ptr, *tok, *data;
    const char *delim = "\r\n";

    if (sz < msg_len)
    {/* not enough data */
        log_debug(ZONE,"Split message packet %d %d",msg_len,sz);
        return 1;
    }

    log_debug(ZONE,"End of message packet %d %d",msg_len,sz);

    data = pmalloc(mp->p,(msg_len + 1) * sizeof(char));
    memcpy(data,buffer,msg_len);
    data[msg_len] = '\0';
    PACKET_ADD(mp->params,mp->count,data);

    if(j_strcmp(mp->params[0],"NOT") == 0)
        return 0; /* End of NOT processing */

    ptr = strstr(data,"\r\n\r\n");
    if(ptr == NULL) {
        log_debug(ZONE,"Failed to find \\r\\n\\r\\n in %s data", mp->params[0]);
        return -1;
    }
    *ptr = '\0';
    ptr += 4;

    tok = strtok(data,delim);
    while ((tok = strtok(NULL,delim)) != NULL)
    {
        PACKET_ADD(mp->params,mp->count,tok);
    }

    if (strcmp(ptr,"\r\n") == 0)
        ptr += 2;

    PACKET_ADD(mp->params,mp->count,ptr);

    return 0;
}

void mt_stream_parse(mpstream st, char *buffer, int sz)
{
    mpacket mp = st->mp;
    char *part = buffer, **params, c;
    int count, i;

    if (mp != NULL)
    {
        params = mp->params;
        count = mp->count;
    }
    else
    {
        params = NULL;
        count = 0;
    }

    for (i = 0; i < sz; i++)
    {
        c = buffer[i];

        if (c == ' ') {

            if (part == NULL)
            {
                log_debug(ZONE,"Parse error!");
                continue;
            }

            if (mp == NULL)
            {
                pool p = pool_new();
                mp = pmalloc(p,sizeof(_mpacket));
                mp->p = p;
            }

            buffer[i] = '\0';
            PACKET_ADD(params,count,pstrdup(mp->p,part));
            part = NULL;

        } else if (c == '\r') {

            if (i + 1 == sz)
                break;

            if (params == NULL || part == NULL || mp == NULL)
            {
                log_debug(NULL,"Parse error %d %d %d",!params,!part,!mp);
//                abort();

                if (params)
                    mt_free(params);
                if (mp)
                    pool_free(mp->p);
                return;
            }

            buffer[i++] = '\0'; /* NULL terminate the last parameter and increment */
            PACKET_ADD(params,count,pstrdup(mp->p,part));

            part = NULL;
            mp->params = params;
            mp->count = count;

            if (j_strcmp(params[0],"MSG") == 0 || j_strcmp(params[0],"NOT") == 0)
            {
                int msg_len = atoi(params[mp->count - 1]);

                i++; /* skip the \n */

                switch (mt_stream_parse_msg(mp,msg_len,buffer + i, sz - i))
                {
                case 1:
                    if (i != sz)
                        part = buffer + i;
                    st->msg_len = msg_len;
                    goto done;

                case 0:
                    i += (msg_len - 1);
                    break;
                case -1:
                    log_debug(ZONE,"Failed to parse message data! %d/%d %s",msg_len,sz - i,buffer);
                    mt_free(params);
                    pool_free(mp->p);
                    return;
                }
            }

            /* finshed with this packet */
            mt_stream_packet(st,mp);

            params = NULL;
            count = 0;
            mp = NULL;

        } else if (part == NULL)
            part = buffer + i;
    }

done:
    if (part != NULL)
    { /* we only have part of this parameter */
        assert(st->buffer == NULL);
        st->buffer = mt_strdup(part);
        st->bufsz = strlen(part);

        log_debug(ZONE,"Saved buffer %s",st->buffer);
    }

    if (mp != NULL)
    {
        assert(params && count);
        mp->count = count;
        mp->params = params;
    }
    st->mp = mp;
}

void mt_stream_more_msg(mpstream st, char *data, int sz)
{
    mpacket mp = st->mp;
    int msg_len = st->msg_len;

    switch (mt_stream_parse_msg(mp,msg_len,data,sz))
    {
    case 1:
        st->buffer = mt_strdup(data);
        st->bufsz = sz;
        break;

    case 0:
        st->mp = NULL;
        st->msg_len = 0;

        mt_stream_packet(st,mp);

        if ((sz -= msg_len) != 0) /* if we have any more data left, parse it */
            mt_stream_parse(st,data + msg_len,sz);
        break;

    case -1:
        mt_free(mp->params);
        pool_free(mp->p);
        st->mp = NULL;
        st->msg_len = 0;
        break;
    }
}

void mt_stream_more(mpstream st, char *new, int sz)
{
    char *data, *old = st->buffer;

    data = mt_malloc(sz + st->bufsz + 1);
    memcpy(data,old,st->bufsz);
    memcpy(data + st->bufsz,new,sz + 1);
    sz += st->bufsz;

    mt_free(old);
    st->buffer = NULL;
    st->bufsz = 0;

    if (st->msg_len)
        mt_stream_more_msg(st,data,sz);
    else
        mt_stream_parse(st,data,sz);

    mt_free(data);
}

void mt_stream_eat(mpstream st, char *data, int sz)
{
    if (st->buffer)
        mt_stream_more(st,data,sz);
    else if (st->msg_len)
        mt_stream_more_msg(st,data,sz);
    else
        mt_stream_parse(st,data,sz);
}


static void mt_stream_read(mio m, int state, void *arg, char *buffer, int bufsz)
{
    mpstream st = (mpstream) arg;

    switch (state)
    {
    case MIO_BUFFER:
        mt_stream_eat(st,buffer,bufsz);
        break;

    case MIO_CLOSED:
        mt_stream_free(st);
        break;
    }
}

static void mt_stream_connecting(mio m, int state, void *arg, char *buffer, int bufsz)
{
    mpstream st = (mpstream) arg;

    switch (state)
    {
    case MIO_NEW:
        if (st->closed == 0)
        {
            log_debug(ZONE,"stream %X connected",st);

            mio_karma(m,KARMA_INIT,KARMA_MAX,KARMA_INC,0,KARMA_PENALTY,KARMA_RESTORE);
            st->m = m;
            if (st->buffer != NULL)
            { /* write data which was buffered before we connected */
                mio_write(m,NULL,st->buffer,st->bufsz);
                mt_free(st->buffer);
                st->buffer = NULL;
                st->bufsz = 0;
            }
            mio_reset(m,&mt_stream_read,(void *) st);
        }
        else
        {
            mio_close(m);
        }
        break;

    case MIO_CLOSED:
        mt_stream_free(st);
        break;
    }
}


void mt_stream_close(mpstream st)
{
    st->closed = 1;
    if (st->m)
        mio_close(st->m);
}

void mt_stream_write(mpstream st, const char *fmt, ...)
{
    va_list ap;
    int ret;

    while (1)
    {
        va_start(ap,fmt);
        ret = vsnprintf (scratch,scratch_sz,fmt,ap);
        va_end(ap);

#ifdef PACKET_DEBUG
        log_debug(ZONE,"WROTE BUFFER %d/%d:%s",ret,scratch_sz,scratch);
#endif
        if (ret == (scratch_sz - 1))
            ret = -1;
        else if (ret > -1 && ret < scratch_sz)
            break;

        if (ret > -1)
            scratch_sz = ret + 1;
        else
            scratch_sz += SCRATCH_INC;

        scratch = mt_realloc(scratch,scratch_sz);
        assert(scratch != NULL);
    }

    ++st->trid;
    if (st->m == NULL)
    {
        assert(st->buffer == NULL);
        st->buffer = mt_strdup(scratch);
        st->bufsz = ret;
    }
    else
        mio_write(st->m,NULL,scratch,ret);
}

void mt_stream_register(mpstream st, handle cb, void *arg)
{
    mphandler ph;

    ph = mt_malloc(sizeof(_mphandler));
    ph->trid = st->trid;
    ph->cb = cb;
    ph->arg = arg;
    ph->next = NULL;

    if (st->head)
        st->tail = st->tail->next = ph;
    else
        st->head = st->tail = ph;
}

mpstream mt_stream_connect(char *host, int port, handle cb, void *arg)
{
    mpstream st;

    st = mt_malloc(sizeof(_mpstream));
    st->cb = cb;
    st->arg = arg;
    st->trid = (1 + (unsigned long) (15.0 * rand() / (RAND_MAX + 1.0)));

    st->m = NULL;
    st->head = st->tail = NULL;
    st->closed = 0;
    st->mp = NULL;
    st->buffer = NULL;
    st->bufsz = st->msg_len = 0;

    mio_connect(host,port,&mt_stream_connecting,(void *) st,0,NULL,NULL);

    return st;
}

void mt_stream_init()
{
    scratch_sz = 1024;
    scratch = mt_malloc(scratch_sz);
}
