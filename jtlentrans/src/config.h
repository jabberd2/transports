#ifndef __CONFIG_H
#define __CONFIG_H

extern int config_init();
extern int config_close();

extern char *config_file;

extern char *config_myjid;
//extern char *config_myfulljid;
extern int config_debug_level;
extern char *config_log_file;
extern int config_syslog;

extern char *config_connect_ip;
extern char *config_connect_id;
extern int config_connect_port;
extern char *config_connect_secret;

extern char *config_dir;

extern char *config_register_instructions;
extern char *config_search_instructions;

extern char *config_gateway_prompt;
extern char *config_gateway_desc;

#endif /* __CONFIG_H */
