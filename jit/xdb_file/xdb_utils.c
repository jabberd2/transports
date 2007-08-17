/* --------------------------------------------------------------------------
   xdb_utils
   by £ukasz Karwacki (C) WP
 * --------------------------------------------------------------------------*/

#include <jabberd.h>
void generate_dir(unsigned char * name,unsigned char * path,int pathsize);

/* simple utility for concat strings */
char *xdb_file_full(int create, pool p, char *spl, char *host, char *file, char *ext)
{
    struct stat s;
    int lenf = strlen(file);
    int lenh = strlen(host);
    int lens = strlen(spl);
    int lene = strlen(ext);
    int index;
    char xdb_file[lenf+10];
    char xdb_path[20];
    char *full = pmalloco(p,lens+lenh+lenf+lene+15);

    memcpy(full,spl,lens);
    index = lens;
    full[index] = '/';
    index++;

    memcpy(full+index,host,lenh);
    index += lenh;
    full[index] = '/';
    index++;

    memcpy(xdb_file,file,lenf);
    xdb_file[lenf]='.';
    memcpy(xdb_file+lenf+1,ext,lene);
    xdb_file[lenf+lene+1] = 0;

    generate_dir(xdb_file,xdb_path,20);

    memcpy(full+index,xdb_path+1,5);
    index+=5;

    /* check dir */
    if (stat(full,&s) == 0) {
      full[index] = '/';
      index++;
      memcpy(full+index,xdb_file,lenf+lene+2);
      return full;
    }

	/* if GET why create */
    if (!create) {
	  return NULL;
	}

    /* try to create directory */
    index = lens+lenh+1;
    full[index] = 0;
    
	/* create host */
    if (stat(full,&s) < 0) {
      mkdir(full, S_IRWXU);
    }
    
    full[index] = '/';
    index+=3;
    full[index] = 0;


	/* create 1 dir */
    if (stat(full,&s) < 0) {
      mkdir(full, S_IRWXU);
    }

    full[index] = '/';
    index+=3;
    full[index] = 0;

	/* create 2 dir */
    if (stat(full,&s) < 0) {
      mkdir(full, S_IRWXU);
    }

    full[index] = '/';
    index++;
    memcpy(full+index,xdb_file,lenf+lene+2);

    return full;
}

/* parse user data */
typedef struct xdb_file_parse_struct {
  pool p;
  xmlnode current;
} *xf_parse,_xf_parse;


void xdb_file_startElement(void* arg, const char* name, const char** atts)
{
    xf_parse xfp = (xf_parse)arg;

    if (xfp->current == NULL)
    {
	  xfp->current = xmlnode_new_tag_pool(xfp->p, name);
	  xmlnode_put_expat_attribs(xfp->current, atts);
    }
    else
    {
	  xfp->current = xmlnode_insert_tag(xfp->current, name);
	  xmlnode_put_expat_attribs(xfp->current, atts);
    }
}

void xdb_file_endElement(void* arg, const char* name)
{
    xf_parse xfp = (xf_parse)arg;
	register xmlnode current = xmlnode_get_parent(xfp->current);

    xfp->current->complete = 1;

	if (current != NULL)
	  xfp->current = current;
}

void xdb_file_charData(void* arg, const char* s, int len)
{
    xf_parse xfp = (xf_parse)arg;

    if (xfp->current != NULL)
	  xmlnode_insert_cdata(xfp->current, s, len);
}

#define READ_BUF_SIZE 8000

xmlnode xdb_file_parse(char *file,pool p)
{
    XML_Parser parser;
    xf_parse xfp;
    char buf[READ_BUF_SIZE];
    int done, fd, len;

    if(NULL == file)
        return NULL;

    fd = open(file,O_RDONLY);
    if(fd < 0)
	  return NULL;

	xfp = pmalloco(p,sizeof(_xf_parse));
	xfp->p = p;

    parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, xfp);
    XML_SetElementHandler(parser, xdb_file_startElement, xdb_file_endElement);
    XML_SetCharacterDataHandler(parser, xdb_file_charData);
    do{
	  len = read(fd, buf, READ_BUF_SIZE);
	  done = len < READ_BUF_SIZE;
	  if(!XML_Parse(parser, buf, len, done)) done = 1;
    }while(!done);
	
    XML_ParserFree(parser);
    close(fd);
    return xfp->current;
}



/* do not create t.m.p file. We don't need this file
   because xdb_file moule is thread safe and there is no way
   to acces the file in many threads 
   
   returns 
    1 write oki
    0 if maxsize reached, not written
	-1 error */
int xdb_file2file(char * file, xmlnode node, int maxsize)
{
  char *doc;
  int fd, i, docsize;
  
  if(file == NULL || node == NULL)
	return -1;
  
  doc = xmlnode2str(node);
  docsize = strlen(doc);

  if (maxsize && (docsize >= maxsize))
	return 0;

  fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0600);
  if(fd < 0)
	return -1;

  i = write(fd,doc,strlen(doc));
  if(i < 0) {
	close(fd);
	return -1;
  }

  close(fd);

  return 1;
}







