#ifndef UTF8_H
#define UTF8_H

int isAscii(const unsigned char *in);
int utf8_to_unicode(const unsigned char *in, unsigned char *out, unsigned short maxlen);
void unicode_to_utf8(const unsigned char *in, int len, unsigned char *out, int maxlen);

#endif

