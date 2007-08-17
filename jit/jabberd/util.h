

#ifdef OTHER
#warning Compiling without safe INC and DEC
#define THREAD_INC(X);  X++;
#define THREAD_DEC(X);  X--;
#else
#define THREAD_INC(X);         \
  asm volatile                 \
    ("\n"                      \
     "lock; incl %0\n\t"       \
     : "=m" (X));
#define THREAD_DEC(X);         \
  asm volatile                 \
    ("\n"                      \
     "lock; decl %0\n\t"       \
     : "=m" (X));
#endif

#define SEM_VAR pthread_mutex_t
#define COND_VAR pthread_cond_t
#define SEM_INIT(x) pthread_mutex_init(&x,NULL)
#define SEM_LOCK(x) pthread_mutex_lock(&x)
#define SEM_UNLOCK(x) pthread_mutex_unlock(&x)
#define SEM_DESTROY(x) pthread_mutex_destroy(&x)
#define COND_INIT(x) pthread_cond_init(&x,NULL)
#define COND_SIGNAL(x) pthread_cond_signal(&x)
#define COND_WAIT(x,s) pthread_cond_wait(&x,&s)

