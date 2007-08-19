#ifndef __ENCODING_H
#define __ENCODING_H

extern char *urlencode(const char *what);
extern char *urldecode(const char *what);
extern char *urlencode2(const char *what);

extern char *unicode_utf8_2_iso88592(const char *src);
extern char *unicode_iso88592_2_utf8(const char *src);

extern char *unicode_utf8d_2_iso88592e(const char *src);
extern char *unicode_iso88592e_2_utf8d(const char *src);

extern int encoding_init();
extern int encoding_done();


#endif /* __ENCODING_H */
