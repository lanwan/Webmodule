/*
 * Copyright 2008 W.Wallucks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

/*
*	RFC 1035 (4. Messages - Seite 24)

    +---------------------+
    |        Header       |
    +---------------------+
    |       Question      | the question for the name server
    +---------------------+
    |        Answer       | RRs answering the question
    +---------------------+
    |      Authority      | RRs pointing toward an authority
    +---------------------+
    |      Additional     | RRs holding additional information
    +---------------------+


The header contains the following fields:

                                    1  1  1  1  1  1
      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      ID                       |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    QDCOUNT                    |	#entries in the question section
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ANCOUNT                    |	#resource records in the answer section
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    NSCOUNT                    |	#server resource records in the authority records section
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ARCOUNT                    |	#resource records in the additional records section
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	12 Bytes


Question section format
                                 1  1  1  1  1  1
      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                                               |	a domain name represented as a sequence of labels, where
    /                     QNAME                     /	each label consists of a length octet followed by that
    /                                               /	number of octets
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     QTYPE                     |	a two octet code which specifies the type of the query
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     QCLASS                    |	a two octet code that specifies the class of the qu
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+


QTYPE           value and meaning

A               1 a host address
NS              2 an authoritative name server
MD              3 a mail destination (Obsolete - use MX)
MF              4 a mail forwarder (Obsolete - use MX)
CNAME           5 the canonical name for an alias
SOA             6 marks the start of a zone of authority
MB              7 a mailbox domain name (EXPERIMENTAL)
MG              8 a mail group member (EXPERIMENTAL)
MR              9 a mail rename domain name (EXPERIMENTAL)
NULL            10 a null RR (EXPERIMENTAL)
WKS             11 a well known service description
PTR             12 a domain name pointer
HINFO           13 host information
MINFO           14 mailbox or mail list information
MX              15 mail exchange
TXT             16 text strings
*               255 A request for all records


QCLASS values
IN              1 the Internet
CS              2 the CSNET class (Obsolete - used only for examples in
                some obsolete RFCs)
CH              3 the CHAOS class
HS              4 Hesiod [Dyer 87]
*				255 any class

Resource record format

                                    1  1  1  1  1  1
      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                                               |
    /                                               /	a domain name to which this resource record pertains
    /                      NAME                     /
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      TYPE                     |	one of the RR type codes
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     CLASS                     |	class of the data
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      TTL                      |
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                   RDLENGTH                    |	length in octets of the RDATA field
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
    /                     RDATA                     /	variable length string of octets that
    /                                               /	describes the resource
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+


*/

#include "config.h"

/**
 * \addtogroup dns	DNS Client
 *
 *	Dieses Modul implementiert einen DNS-Client zur Namensauflösung
 *	von Internet-Hostnamen.
 *
 *	Da die DNS-Anfragen über UDP erfolgen, ist es unzweckmäßig in der
 *	anfragenden Funktion auch auf die Antwort zu warten. Dies würde
 *	eventuell einen längeren Timeout bedeuten. Daher kann die erhaltene
 *	IP der Namensauflösung an einer anzugebenden Stelle oder in der
 *	Variablen <tt>dnsQueryIP</tt> gespeichert werden. Vor der Anfrage
 *	setzt man diese Speicherstelle auf null und kann dann regelmäßig in
 *	einer Schleife pollen, ob die Namensauflösung bereits erfolgreich ist.
 *	Laut RFC sollte hier ein Timeout von 2 bis 3 Sekunden gesetzt werden,
 *	nach dem dann die Anfrage eventuell wiederholt werden kann.
 *
 *  Bei der Anfrage müssen hostnamen als "Fully Qualified Domain Name" (FQDN)
 *	angegeben werden.
 *
 *	Ein Anwendungsbeispiel der DNS-Auflösung ist in sendmail() codiert. In
 *	<tt>SENDMAIL_STATUS sm_status</tt> wird in <tt>server_ip</tt> die IP
 *	des SMTP-Servers gespeichert. Wenn dieser Wert noch null ist, wird
 *	zuerst eine DNS-Anfrage abgesetzt und bei wiederholtem Aufruf der Wert
 *	von <tt>timeout</tt> ausgewertet um erneut anzufragen oder weiter zu warten.
 *	Erst wenn eine DNS-Antwort die IP des SMTP-Servers gesetzt hat, wird
 *	der DNS-Code in sendmail übersprungen und das Versenden der E-Mail kann
 *	mit den folgenden Schritten fortfahren.
 *
 *	\sa cmd_DNS	CMD um DNS-Server abzufragen / zu setzen<br>
 *		cmd_DNSQ	CMD um DNS-Anfrage auszulösen
 *
 *	Die Aktivierung erfolgt mittels #USE_DNS in der config.h
 * @{
 */

/**
 * \file
 * dns.c
 *
 * \author W.Wallucks
 */

/**
 * \addtogroup dnsintern	interne Hilfsfunktionen
 */

/**
 * @}
 */
#if USE_DNS || DOXYGEN

#include <string.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "stack.h"
#include "usart.h"
#include "dns.h"
#include "timer.h"


#define DNS_DEBUG usart_write
//#define DNS_DEBUG(...)

/*
*	Variable
*/
uint8_t  dns_server_ip[4];	//!< IP des DNS-Servers
uint32_t dnsQueryIP;		//!< Speicherplatz für gefundene IP
static uint16_t dnsQueryId;

#define DNS_HEADER_LEN	12	// Länge des DNS Headers

/*
*	Prototypen
*/
void _dns_qry(uint8_t, char *, void *);
void dns_get(unsigned char);

//----------------------------------------------------------------------------
/**
*	\ingroup dns
*	Initialisierung des DNS Client Ports (für Datenempfang)
*/
void dns_init (void)
{
	// Port in Anwendungstabelle eintragen für eingehende DNS Daten!
	add_udp_app(DNS_CLIENT_PORT, dns_get);
	
	// DNS IP aus EEPROM auslesen
	(*((unsigned long*)&dns_server_ip[0])) = get_eeprom_value(DNS_IP_EEPROM_STORE,DNS_IP);
	
	return;
}

//----------------------------------------------------------------------------
/**
 *	\ingroup dns
 *	DNS request an einen DNS-Server senden
 *
 *	\param[in] hostname FQDN des gesuchten Servers
 *	\param[in] ip Speicherplatz für gefundene IP
*/
void dns_request(char *hostname, uint32_t *ip)
{
	_dns_qry(1, hostname, ip);	// A-Record anfragen
}


//----------------------------------------------------------------------------
/**
 *	\ingroup dns
 *	DNS Reverse Lookup an einen DNS-Server senden
 *
 *	\param[in] ip aufzulösende IP Adresse
 *	\param[in] hostname Speicherplatz für hostnamen des gesuchten Servers
*/
void dns_reverse_request(uint32_t ip, char *hostname)
{
	char ipstring[30];

	sprintf_P(ipstring,PSTR("%i.%i.%i.%i.in-addr.arpa"),(uint16_t)((ip&0xFF000000)>>24),
													    (uint16_t)((ip&0x00FF0000)>>16),
													    (uint16_t)((ip&0x0000FF00)>>8),
													    (uint16_t)(ip&0x000000FF));
	_dns_qry(12, ipstring, hostname);	// PTR-Record anfragen
}

//----------------------------------------------------------------------------
/**
*	\ingroup dnsintern
 *	lokale Arbeitsfunktion für DNS-Anfrage
 *
 *	\param[in] qtype QTYPE Anfragerecord
 *	\param[in] hostname FQDN des gesuchten Servers
 *	\param[in] dest Speicherplatz für gefundene IP
*/
void _dns_qry(uint8_t qtype, char *hostname, void *dest)
{
	DNS_DEBUG("\r\nDNS Anfrage: %s",hostname);
	// ARP Request senden
	unsigned long tmp_ip = (*(unsigned long*)&dns_server_ip[0]);
	if (arp_request(tmp_ip) == 1) {

		memset(&eth_buffer[UDP_DATA_START], 0, DNS_HEADER_LEN);

		// dnsQueryId = (uint16_t)(time & 0xffff);		// Zufallszahl
		// Speicheradresse für aufgelöste IP/hostname
		// als QueryID mitgeben
		dnsQueryId = (uint16_t)dest;
		eth_buffer[UDP_DATA_START+0] = (uint8_t)(dnsQueryId>>8 & 0xff);
		eth_buffer[UDP_DATA_START+1] = (uint8_t)(dnsQueryId & 0xff);
		eth_buffer[UDP_DATA_START+2] = 1;	// Recursion Desired
		eth_buffer[UDP_DATA_START+5] = 1;	// eine Anfrage

		char *ptr = &eth_buffer[UDP_DATA_START+DNS_HEADER_LEN];
		uint8_t *src = (uint8_t *)hostname;		// Namen kopieren
		uint8_t *pdot;
		
		// aus "dotted" hostname den QNAME erstellen
		do {
			pdot = (uint8_t *)strchrnul((char *)src,'.');
			*ptr++ = (uint8_t)(pdot-src);
			while (src < pdot)
				*ptr++ = *src++;
			src++;
		} while ( *pdot );
		*ptr++ = 0;

		*ptr++ = 0;		// Question type
		*ptr++ = qtype;

		*ptr++ = 0;		// Question class
		*ptr++ = 1;		// IN - Internet

		create_new_udp_packet((unsigned int)(ptr - &eth_buffer[UDP_DATA_START]),
								DNS_CLIENT_PORT,DNS_SERVER_PORT,tmp_ip);
		DNS_DEBUG("\r\n** DNS Request gesendet! **");
		return;
	}

	DNS_DEBUG("\r\nKeinen DNS Server gefunden!!");
}

//----------------------------------------------------------------------------
/**
*	\ingroup dnsintern
*	Empfang der DNS Informationen von einem DNS-Server
*/
void dns_get(unsigned char index)
{
	DNS_DEBUG("** DNS DATA GET! **\r\n");

	uint16_t id = (uint16_t)((eth_buffer[UDP_DATA_START+0]<<8) + eth_buffer[UDP_DATA_START+1]);

	if (id != dnsQueryId) {
		DNS_DEBUG("\r\n** DNS falsche ID: 0x%x 0x%x", dnsQueryId, id);
		return;			// das war nicht meine Anfrage
	}

	// assume max. 255 resource records in the answer section
	// MX Anfragen senden hostnamen und IPs in additional records section
	uint8_t ancount = eth_buffer[UDP_DATA_START+7] + eth_buffer[UDP_DATA_START+11];
	DNS_DEBUG("\r\n** DNS: %i RRs",ancount);
		
	// Zeiger auf Question section setzen
	char *ptr = &eth_buffer[UDP_DATA_START + DNS_HEADER_LEN];

	while (*ptr++);	// ans Ende des hostnamen zählen
	ptr += 4;		// QTYPE und QCLASS überspringen

	uint8_t qtype;	// RR typedns
	uint8_t qclass;	// RR class
	uint16_t rdlen;	// RDLength

	// Schleife über alle RR in answer section
	for (uint8_t i=0; i<ancount; ++i) {
		if (*ptr >= 0xc0) {
			ptr += 2;		// Message compression offset überspringen
		}
		else {
			while (*ptr++);	// ans Ende des hostnamen zählen
		}

		qtype	= *(ptr+1);
		qclass	= *(ptr+3);
		rdlen	= *(ptr+9) + ((*(ptr+8))<<8);
		ptr += 10;	// Type (2); Class (2); TTL (4); RDLength (2)
		DNS_DEBUG("\r\n** DNS TYPE %i CLASS %i RDLen %i",qtype, qclass, rdlen);

		if (qtype == 1) {		// A		: 1 a host address
			// die nächsten 4 Bytes sind die IP des host
			DNS_DEBUG("\r\n** DNS IP empfangen: %i.%i.%i.%i",(int)*ptr,(int)*(ptr+1),(int)*(ptr+2),(int)*(ptr+3));
			uint32_t *ip = (uint32_t *)id;
			*ip = IP(*(ptr+0), *(ptr+1), *(ptr+2),  *(ptr+3));
			// dnsQueryIP = IP(*(ptr+0), *(ptr+1), *(ptr+2),  *(ptr+3));
		}
		else if (qtype == 2) {	// NS		: 2 an authoritative name server
			DNS_DEBUG("\r\n** NS empfangen: %s",ptr);
		}
		else if (qtype == 5) {	// CNAME	: 5 the canonical name for an alias
			DNS_DEBUG("\r\n** CNAME empfangen: %s",ptr);
		}
		else if (qtype == 6) {	// SOA		: 6 marks the start of a zone of authority
			DNS_DEBUG("\r\n** SOA empfangen: %s",ptr);
		}
		else if (qtype == 12) {	// PTR		: 12 a domain name pointer
			// QNAME zu "dotted hostname" umwandeln
			char *hostname = (char *)id;
			uint8_t pdot;
			char *host = ptr;
			uint8_t max = 32;

			while( *host && --max>0) {
				pdot = *host++;
				while(pdot>0 && --max>0) {
					*hostname++ = *host++;
					--pdot;
				}
			*hostname++ = '.';
			}

			--hostname;
			*hostname = '\0';

			DNS_DEBUG("\r\n** PTR empfangen: %s",(char *)id);
		}
		else if (qtype == 15) {	// MX		: 15 mail exchange
			DNS_DEBUG("\r\n** MX empfangen: %s",ptr);
		}

		ptr += rdlen;
	}
}

#endif
