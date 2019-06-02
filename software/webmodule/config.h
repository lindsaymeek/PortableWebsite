
#ifndef CONFIG_H
#define CONFIG_H

#include "kernel.h"

struct CONFIG {

	char *isp_phone;
	char *isp_username;
	char *isp_password;

	u8   dyndns_server[4]; // also used for the keepalive ping
	u16	 dyndns_port;
	char *dyndns_password;
	char *domain;

	long baudrate;
	char dial_enabled;
	char dyndns_type;
	} ;

#define DYNDNS_TYPE_DYNU 0

extern struct CONFIG config;

bool config_init(bool disk_ok);

#endif
