/*
 * Copyright 2008 W.Wallucks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

/**
 * \addtogroup dns	DNS Client
 *
 * @{
 */

/**
 * \file
 * dns.h
 *
 * \author W.Wallucks
 */

/**
 * @}
 */
#ifndef _DNSCLIENT_H
	#define _DNSCLIENT_H

	#define DNS_CLIENT_PORT			5300	//!< UDP Port # des clients
	#define DNS_SERVER_PORT			53		//!< UDP Portnummer des DNS-Servers
	#define DNS_IP_EEPROM_STORE 	42		//!< EEPROM Speicherplatz der DNS-Server IP

	extern uint8_t dns_server_ip[4];
	extern uint32_t dnsQueryIP;
	
	void dns_init(void);
	void dns_request(char *, uint32_t *);
	void dns_reverse_request(uint32_t, char *);

#endif

