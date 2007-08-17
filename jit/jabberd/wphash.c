/* --------------------------------------------------------------------------
  WPHash

  Author: £ukasz Karwacki <lukasm@wp-sa.pl>

  Element putted into hash must have bucket definition at the begining.


 * --------------------------------------------------------------------------*/

#include <jabberd.h>

/* Generates a hash code for a string.
 * This function uses the ELF hashing algorithm as reprinted in 
 * Andrew Binstock, "Hashing Rehashed," Dr. Dobb's Journal, April 1996.
 */
int _wpxhasher(const char *s)
{
    const unsigned char *name = (const unsigned char *)s;
    unsigned long h = 0, g;

    while (*name)
    { 
        h = (h << 4) + (unsigned long)(*name++);
        if ((g = (h & 0xF0000000UL))!=0)
            h ^= (g >> 24);
        h &= ~g;

    }

    return (int)h;
}

inline wpxhn _wpxhash_node_get(wpxht h, const char *key, int index)
{
    wpxhn n;

    int i = index % h->prime;
    for(n = h->zen[i]; n != NULL; n = n->wpnext)
        if(j_strcmp(key, n->wpkey) == 0)
            return n;
    return NULL;

}

/* init */
wpxht wpxhash_new(int prime)
{
    wpxht xnew;
    pool p;

    p = pool_new(); 
    xnew = pmalloco(p, sizeof(_wpxht) + sizeof(wpxhn)*prime);
    xnew->prime = prime;
    xnew->p = p;
    return xnew;
}

/* init + alloc extra memory */
wpxht wpxhash_new_extra(int prime, int extra)
{
    wpxht xnew;
    pool p;

    p = pool_heap(extra); 
    xnew = pmalloco(p, sizeof(_wpxht) + sizeof(wpxhn)*prime);
    xnew->prime = prime;
    xnew->p = p;
    return xnew;
}


/*
  val must be wpxhn + other stuff
  wpkey will be updated


  put will replace !!!
*/
int wpxhash_put(wpxht h, const char *key, void *val)
{
    wpxhn n,lastn;
	int i;

    if(h == NULL || key == NULL)
        return 0;

    i = _wpxhasher(key) % h->prime;
	
	if (h->zen[i] == NULL) {
	  h->zen[i] = (wpxhn)val;
	  ((wpxhn)val)->wpkey = key;
	  return 1;
	}

	lastn = NULL;
    for(n = h->zen[i]; n != NULL; n = n->wpnext) {
	  if(strcmp(key, n->wpkey) == 0)
		break;
	  lastn = n;
	}
	
	/* if found n is founded, lastn is last element on n */       
	if (n != NULL) {
	  /* replace old */

	  /* if not first */
	  if (lastn) 
		lastn->wpnext = (wpxhn)val;
	  else 
		h->zen[i] = (wpxhn)val;

	  /* set data */
	  ((wpxhn)val)->wpnext = n->wpnext;

	  ((wpxhn)val)->wpkey = key;
	  return 1;
	}
	
	lastn->wpnext = (wpxhn)val;
	((wpxhn)val)->wpkey = key;

	return 1;
}



void *wpxhash_get(wpxht h, const char *key)
{
  if(h == NULL || key == NULL)
	return NULL;
  
  return _wpxhash_node_get(h, key, _wpxhasher(key));
}


void wpxhash_zap(wpxht h, const char *key)
{
    wpxhn n,lastn;
	int i;

    if(h == NULL || key == NULL)
        return;

    i = _wpxhasher(key) % h->prime;
	
	lastn = NULL;
    for(n = h->zen[i]; n != NULL; n = n->wpnext) {
	  if(strcmp(key, n->wpkey) == 0)
		break;
	  lastn = n;
	}
	
	if (n == NULL)
	  return;

	/* if not first */
	if (lastn)
	  lastn->wpnext = n->wpnext;
	else
	  h->zen[i] = n->wpnext;
}

void wpxhash_free(wpxht h)
{
  if(h != NULL)
	pool_free(h->p);
}

void wpxhash_walk(wpxht h, wpxhash_walker w, void *arg)
{
    int i;
    wpxhn n,t;

    if(h == NULL || w == NULL)
        return;

    for(i = 0; i < h->prime; i++)
	  for(n = h->zen[i]; n != NULL; ) {
		t = n;
		n = t->wpnext;
		(*w)(h, t->wpkey, t, arg);
	  }
}

void wpghash_walk(wpxht h, wpghash_walker w, void *arg)
{
    int i;
    wpxhn n,t;

    if(h == NULL || w == NULL)
        return;

    for(i = 0; i < h->prime; i++)
	  for(n = h->zen[i]; n != NULL; ) {
		t = n;
		n = t->wpnext;
		(*w)(arg, t->wpkey, t);
	  }
}

pool wpxhash_pool(wpxht h) {
  return h->p;
}
