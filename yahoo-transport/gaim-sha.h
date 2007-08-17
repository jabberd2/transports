/*
#if (SIZEOF_INT == 4)
typedef unsigned int uint32;
#elif (SIZEOF_SHORT == 4)
typedef unsigned short uint32;
#else
typedef unsigned int uint32;
#endif  HAVEUINT32 */

int strprintsha(char *dest, int *hashval);
 
typedef struct {
  unsigned long H[5];
  unsigned long W[80];
  int lenW;
  unsigned long sizeHi,sizeLo;
} GAIM_SHA_CTX;
 
void gaim_shaInit(GAIM_SHA_CTX *ctx);
void gaim_shaUpdate(GAIM_SHA_CTX *ctx, unsigned char *dataIn, int len);
void gaim_shaFinal(GAIM_SHA_CTX *ctx, unsigned char hashout[20]);
void gaim_shaBlock(unsigned char *dataIn, int len, unsigned char hashout[20]);
