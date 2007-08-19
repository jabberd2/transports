#include <stdlib.h>
#include <libxode.h>

#include "all.h"

#include "config.h"

char *config_file = NULL;

char *config_myjid = NULL;
//char *config_myfulljid = NULL;
char *config_serverjid = NULL;
int config_debug_level = 4;
char *config_log_file = NULL;
int config_syslog = 0;
char *pid_file = NULL;

char *config_connect_ip = NULL;
int config_connect_port;
char * config_connect_id = NULL;

char *config_connect_secret = NULL;

char *config_dir = NULL;

char *config_register_instructions = NULL;
char *config_search_instructions = NULL;

char *config_gateway_prompt = NULL;
char *config_gateway_desc = NULL;

int config_init() {
	xode cfg, x;
	char *temp;

	if (config_file == NULL)
		return -1;

	if (!(cfg = xode_from_file(config_file))) {
		my_debug(0, "config: Nie mozna otworzyc pliku konfiguracyjnego");
		return -1;
	}
	
	temp = xode_get_name(cfg);
	if (temp == NULL || strcmp(temp, "tt")) {
		my_debug(0, "config: Bledny format pliku");
		return -1;
	}

	if (!(x = xode_get_tag(cfg, "service"))) {
		my_debug(0, "config: Brak tagu service");
		return -1;
	} else {
		if (!(temp = xode_get_attrib(x, "jid"))) {
			my_debug(0, "config: Brak parametru jid");
			return -1;
		}
		my_strcpy(config_myjid, temp);
		//my_strcpy(config_myfulljid, temp);
		/*my_strcat(config_myfulljid, temp, "/registered");*/
	}

	if (!(x = xode_get_tag(cfg, "connect/ip"))) {
		my_debug(0, "config: Brak tagu connect/ip");
		return -1;
	}
	temp = xode_get_data(x);
	my_strcpy(config_connect_ip, temp);

	if (!(x = xode_get_tag(cfg, "connect/port"))) {
		my_debug(0, "config: Brak tagu connect/port");
		return -1;
	}
	config_connect_port = atoi(xode_get_data(x));

	if (!(x = xode_get_tag(cfg, "connect/secret"))) {
		my_debug(0, "config: Brak tagu connect/secret");
		return -1;
	}
	temp = xode_get_data(x);
	my_strcpy(config_connect_secret, temp);

	if (!(x = xode_get_tag(cfg, "connect"))) {
		my_debug(0, "config: Brak tagu connect");
		return -1;
	} else {
		if (!(temp = xode_get_attrib(x, "id"))) {
			my_debug(0, "config: Brak parametru id");
			return -1;
		}
		my_strcpy(config_connect_id, temp);
	}

	if ((x = xode_get_tag(cfg, "debug"))) {
		config_debug_level = atoi(xode_get_attrib(x, "loglevel")); /* FIXIT */
	} else {
		my_debug(0, "config: Brak tagu debug loglevel, assuming 4");
		config_debug_level = 4;
	}

	x = xode_get_tag(cfg, "log");
	if ((x = xode_get_tag(cfg, "log"))) {
		int a;
		a = 0;
		if ((x = xode_get_tag(cfg, "log?type=file"))) {
			temp = xode_get_data(x);
			my_strcpy(config_log_file, temp);
			a = 1;
		}
		if ((x = xode_get_tag(cfg, "log?type=syslog"))) {
			config_syslog = 1;
			a = 1;
		}
		if (a == 0) { /* FIXIT */
			my_debug(0, "config: Nieznany typ logowania %s", temp);
			exit(1);
		}
	}

	if (!(x = xode_get_tag(cfg, "spool"))) {
		my_debug(0, "config: Brak tagu spool");
		return -1;
	}
	temp = xode_get_data(x);
	my_strcpy(config_dir, temp);

	x = xode_get_tag(cfg, "register/instructions");
	temp = xode_get_data(x);
	my_strcpy(config_register_instructions, temp);

	x = xode_get_tag(cfg, "search/instructions");
	temp = xode_get_data(x);
	my_strcpy(config_search_instructions, temp);

	x = xode_get_tag(cfg, "gateway/desc");
	temp = xode_get_data(x);
	my_strcpy(config_gateway_desc, temp);

	x = xode_get_tag(cfg, "gateway/prompt");
	temp = xode_get_data(x);
	my_strcpy(config_gateway_prompt, temp);

	x = xode_get_tag(cfg, "pidfile");
	temp = xode_get_data(x);
	my_strcpy(pid_file, temp);

	xode_free(cfg);

	my_debug(2, "config: Oto wczytana konfiguracja");
	my_debug(2, "config: config_myjid: %s", config_myjid);
	my_debug(2, "config: config_connect_ip: %s", config_connect_ip);
	my_debug(2, "config: config_connect_port: %d", config_connect_port);
	my_debug(2, "config: config_connect_secret: %s", config_connect_secret);
	my_debug(2, "config: config_connect_jid: %s", config_connect_id);
	my_debug(2, "config: config_dir: %s", config_dir);
	my_debug(2, "config: config_log_file: %s", config_log_file);
	my_debug(2, "config: config_syslog: %d", config_syslog);
	my_debug(2, "config: config_debug_level: %d", config_debug_level);

	return 0;
}

int config_close() {
	t_free(pid_file);
	t_free(config_myjid);
	t_free(config_connect_ip);
	t_free(config_connect_secret);
	t_free(config_connect_id);
	t_free(config_dir);
	t_free(config_gateway_prompt);
	t_free(config_gateway_desc);
	t_free(config_search_instructions);
	t_free(config_register_instructions);
	t_free(config_log_file);

	return 0;
}
