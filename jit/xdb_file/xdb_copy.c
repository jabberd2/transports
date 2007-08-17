/* --------------------------------------------------------------------------
   xdb_copy utility
   by £ukasz Karwacki (C) WP
   
   
   usage
   
   create dir spool1 to have
   
   ./spool
   ./spool1
   
   
   then run find command to search in spool dir and execute xdb_copy found_file_name
   
   xdb_copy will create dirs and copy file to spool1
   
 * --------------------------------------------------------------------------*/
 
#include <bits/time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/timeb.h>
#include <sys/ioctl.h>

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>

void generate_dir(char * name,char * path,int pathsize);

int main (int argc, char** argv)
{
  struct stat s;
  char buf[500];
  char sciezka[500];
  char * a,*b;
  char *f;
  char *t[10];
  int index = 0;
  void * start;
  int fd,fd2;
  

  if (argc != 2) {
    fprintf(stderr,"xdb_copy needs parameter.\r\n");
    return 0;
  }

  /* build path */
  /* we have */
  /*                 0            1             */
  /*  ./spool/offline/jabber.wp.pl/a/aa/ala.xml */
  a = argv[1];
  b = sciezka;
  a += 6;
  memcpy(b,"./spool1/",9);
  b +=9;
  *b=0;

  while(1) {
    if (*a == 0) {
      break;
    }

    if (*a == '/') {
      f = a;

      if (index < 2) {
	*b = 0;

	//fprintf(stderr,"create >%s<\r\n",sciezka);
	if (stat(sciezka,&s) < 0)
	  mkdir(sciezka, S_IRWXU);
	
	t[index] = b;
	index++;
	
	*b = *a;
	b++;
      }
      a++;
      continue;
    }

    if (index < 2) {
      *b = *a;
      b++;
    }
    a++;
  }

  /* generate path_info */
  f++;
  generate_dir(f,buf,200);

  memcpy(b,buf+1,2);
  b+=2;
  *b=0;

  // fprintf(stderr,"create >%s<\r\n",sciezka);
  if (stat(sciezka,&s) < 0)
   mkdir(sciezka, S_IRWXU);

  *b='/';
  memcpy(b+1,buf+4,2);
  b+=3;
  *b=0;

  //  fprintf(stderr,"create >%s<\r\n",sciezka);
  if (stat(sciezka,&s) < 0)
    mkdir(sciezka, S_IRWXU);

  *b='/';
  memcpy(b+1,f,a-f);
  b+=a-f+1;
  *b=0;

  /* copy file */
  if (stat(argv[1],&s) < 0) {
    fprintf(stdout,"error >%s<\r\n",argv[1]);
    return 0;
  }

  fd = open(argv[1],O_RDONLY);
  start = mmap(NULL,s.st_size,PROT_READ,MAP_SHARED,fd,0);
  
  fd2 = open(sciezka,O_TRUNC | O_CREAT | O_WRONLY,S_IRUSR|S_IWUSR|S_IRGRP);
  write(fd2,start,s.st_size);
  close(fd2);

  munmap(start,s.st_size);
  close(fd);

  fprintf(stdout,"%s\r\n",sciezka);
  
  return 0;
}







