
#ifdef __cplusplus
extern "C" {
#endif


/* --------------------------------------------------------- */
/*                                                           */
/* WP Hashtable                                              */
/*                                                           */
/* --------------------------------------------------------- */

#define WPHASH_BUCKET struct wpxhn_struct *wpnext; \
const char *wpkey
  
typedef struct wpxhn_struct
{
   WPHASH_BUCKET;
} *wpxhn, _wpxhn;

typedef struct wpxht_struct
{
    pool p;
    int prime;
    wpxhn zen[1];
} *wpxht, _wpxht;

wpxht wpxhash_new(int prime);
wpxht wpxhash_new_extra(int prime, int extra);
int wpxhash_put(wpxht h, const char *key, void *val);
void *wpxhash_get(wpxht h, const char *key);
void wpxhash_zap(wpxht h, const char *key);
void wpxhash_free(wpxht h);
typedef int (*wpghash_walker)(void *user_data, const void *key, void *data);
typedef void (*wpxhash_walker)(wpxht h, const char *key, void *val, void *arg);
void wpxhash_walk(wpxht h, wpxhash_walker w, void *arg);
void wpghash_walk(wpxht h, wpghash_walker w, void *arg);

#ifdef __cplusplus
}
#endif
