/* --------------------------------------------------------------------------
   xdb_path generator
   by £ukasz Karwacki (C) WP
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
  char buf[200];

  if (argc != 2) {
    fprintf(stderr,"xdb_path needs parameter.\r\n");
    fprintf(stderr,"\r\nExample:\r\n\t\t xdb_path rabbit.xml\r\n");    
    return 0;
  }

  /* generate path_info */
  generate_dir(argv[1],buf,200);

  fprintf(stdout,"%s",buf);
  
  return 0;
}
