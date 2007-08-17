/*
    JCR - Jabber Component Runtime
    Copyright (C) 2003 Paul Curtis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

$Id: main.c,v 1.5 2004/07/02 17:09:33 pcurtis Exp $

*/

#include "jcomp.h"

int main(int argc, char *argv[]) {
  extern char *optarg;
  extern int optind, opterr, optopt;
  int inBackground = 0;
  int rc, pid, c;
  int message_mask_set = 0;
  int message_stderr_set = 0;
  int fd = 0, fdlimit = 0;
  struct sigaction act;
  FILE *pid_stream;
  struct stat st;
  char *config_file = NULL;
  pool p;
  GThread       *dthread; /* the packet delivery thread */
  GMainLoop     *gmain;   /* the receive packet event loop */
  jcr = (jcr_instance)malloc(sizeof(_jcr_instance));


  g_thread_init(NULL);
  fprintf(stderr, "%s -- %s\n%s\n\n", _JCOMP_NAME, _JCOMP_VERS, _JCOMP_COPY);

  while ((c = getopt(argc, argv, "Bsd:c:")) != EOF)
    switch (c) {
    case 'B':
      inBackground = 1;
      break;

    case 'c':
      config_file = optarg;
      break;

    case 'd':
      jcr->message_mask = j_atoi(optarg, 124);
      message_mask_set = 1;
      break;

    case 's':
      jcr->message_stderr = 1;
      message_stderr_set = 1;
      break;

    }


  /* The configuration file must be specified, and there is no default */
  if (config_file == NULL) {
    fprintf(stderr, "%s: Configuration file not specified, exiting.\n", JDBG);
    return 1;
  }

  /* Parse the XML in the config file -- store it as a node */
  jcr->config = xmlnode_file(config_file);
  if (jcr->config == NULL) {
    fprintf(stderr, "%s: XML parsing failed in configuration file '%s'\n%s\n", JDBG, config_file, xmlnode_file_borked(config_file));
    return 1;
  }

  /* The spool directory --- for all xdb calls. */
  if ((xmlnode_get_type(xmlnode_get_tag(jcr->config,"spool")) == NTYPE_TAG) == 0) {
    fprintf(stderr, "%s: No spool directory specified in configuration file '%s'\n", JDBG, config_file);
    return 1;
  }
  jcr->spool_dir = strdup(xmlnode_get_data(xmlnode_get_tag(jcr->config,"spool")));
  rc = stat(jcr->spool_dir, &st);
  if (rc < 0) {
    fprintf(stderr, "%s: <spool> '%s': ", JDBG, jcr->spool_dir);
    perror(NULL);
    return 1;
  }
  if (!(S_ISDIR(st.st_mode))) {
    fprintf(stderr, "%s: <spool> '%s' is not a directory.\n", JDBG, jcr->spool_dir);
    return 1;
  }

  /* The log directory --- for the log_* functions */
  if ((xmlnode_get_type(xmlnode_get_tag(jcr->config,"logdir")) == NTYPE_TAG) == 0) {
    fprintf(stderr, "%s: No log directory specified in configuration file '%s'\n", JDBG, config_file);
    return 1;
  }
  jcr->log_dir = strdup(xmlnode_get_data(xmlnode_get_tag(jcr->config,"logdir")));
  rc = stat(jcr->log_dir, &st);
  if (rc < 0) {
    fprintf(stderr, "%s: <logdir> '%s': ", JDBG, jcr->log_dir);
    perror(NULL);
    return 1;
  }
  if (!(S_ISDIR(st.st_mode))) {
    fprintf(stderr, "%s: <logdir> '%s' is not a directory.\n", JDBG, jcr->log_dir);
    return 1;
  }

  if (!message_mask_set)
    jcr->message_mask = j_atoi(xmlnode_get_data(xmlnode_get_tag(jcr->config,"loglevel")), 124);
  if (!message_stderr_set)
    jcr->message_stderr = (xmlnode_get_type(xmlnode_get_tag(jcr->config,"logstderr")) == NTYPE_TAG);


  if (inBackground == 1) {
    if ((pid = fork()) == -1) {
      fprintf(stderr, "%s: Could not start in background\n", JDBG);
      exit(1);
    }
    if (pid)
      exit(0);

    /* in child process .... process and terminal housekeeping */
    setsid();
    fdlimit = sysconf(_SC_OPEN_MAX);
    while (fd < fdlimit)
      close(fd++);
    open("/dev/null",O_RDWR);
    dup(0);
    dup(0);
  }
  pid = getpid();

  /* We now can initialize the resources */
  g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                     | G_LOG_FLAG_RECURSION, jcr_log_handler, jcr);


  log_warn(JDBG, "%s -- %s  starting.", _JCOMP_NAME, _JCOMP_VERS);

  config_file = xmlnode_get_data(xmlnode_get_tag(jcr->config,"pidfile"));
  if (config_file == NULL)
    config_file = "./jcr.pid";
  pid_stream = fopen(config_file, "w");
  if (pid_stream != NULL) {
    fprintf(pid_stream, "%d", pid);
    fclose(pid_stream);
  }
  jcr->pid_file = j_strdup(config_file);

  jcr->dqueue = g_async_queue_new();
  gmain = g_main_loop_new(NULL, FALSE);
  jcr->gmain = gmain;
  jcr->recv_buffer_size = j_atoi(xmlnode_get_data(xmlnode_get_tag(jcr->config,"recv-buffer")), 8192);
  jcr->recv_buffer = (char *)malloc(jcr->recv_buffer_size);
  jcr->in_shutdown = 0;

  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGTERM);
  sigaddset(&act.sa_mask, SIGINT);
  sigaddset(&act.sa_mask, SIGKILL);
  act.sa_handler = jcr_server_shutdown;
//act.sa_restorer = NULL;
  act.sa_flags = 0;

  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGKILL, &act, NULL);

  p = pool_new();
  jcr->jcr_i = (instance)pmalloc(p, sizeof(instance));
  jcr->jcr_i->p = p;
  jcr->jcr_i->id = pstrdup(p, xmlnode_get_data(xmlnode_get_tag(jcr->config,"host")));

  /* The component call */
  yahoo_transport(jcr->jcr_i, NULL);


  jcr->fd = -1;

  while(jcr_socket_connect()) {
    sleep(2);
  }

  log_warn(JDBG, "Main loop starting.");
  jcr_main_new_stream();
  g_main_loop_run(gmain);
  log_warn(JDBG, "Main loop exiting.");

  return 0;
}
