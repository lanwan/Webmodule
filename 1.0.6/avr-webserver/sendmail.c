/*------------------------------------------------------------------------------
 Copyright:      Radig Ulrich + Wilfried Wallucks
 Author:         Radig Ulrich + W.Wallucks
 Remarks:        
 known Problems: none
 Version:        09.06.2008
 Description:    sendmail - E-Mail client

 Dieses Programm ist freie Software. Sie können es unter den Bedingungen der 
 GNU General Public License, wie von der Free Software Foundation veröffentlicht, 
 weitergeben und/oder modifizieren, entweder gemäß Version 2 der Lizenz oder 
 (nach Ihrer Option) jeder späteren Version. 

 Die Veröffentlichung dieses Programms erfolgt in der Hoffnung, 
 daß es Ihnen von Nutzen sein wird, aber OHNE IRGENDEINE GARANTIE, 
 sogar ohne die implizite Garantie der MARKTREIFE oder der VERWENDBARKEIT 
 FÜR EINEN BESTIMMTEN ZWECK. Details finden Sie in der GNU General Public License. 

 Sie sollten eine Kopie der GNU General Public License zusammen mit diesem 
 Programm erhalten haben. 
 Falls nicht, schreiben Sie an die Free Software Foundation, 
 Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA. 
-------------------------------------------------------------------------------*/
#include "config.h"

#if USE_MAIL || DOXYGEN

#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <string.h>
#include <ctype.h>
#include <avr/pgmspace.h>

#include "base64.h"
#include "sendmail.h"
#include "sdkarte/sdcard.h"
#include "dns.h"
#include "stack.h"
#include "timer.h"
#include "translate.h"


#include "usart.h"
//#define MAIL_DEBUG usart_write	//mit Debugausgabe
#define MAIL_DEBUG(...)		//ohne Debugausgabe

/**
 * \addtogroup mail	Sendmail Client
 *	Dieses Modul implementiert einen Sendmail Client der unterschiedliche
 *	Nachrichten versenden kann, die sich in einer Datei (mail.ini) auf
 *	der SD-Karte befinden müssen.
 *
 *	Das Format der ini-Datei ist wie folgt:
 *	- angegeben werden muss
 *	  - der empfangende Mailserver (FQDN),<br>
 *	  - die Absender-Mailadresse in spitzen Klammern,<br>
 *	  - der Benutzer zur Authentifizierung beim Mailserver<br>
 *	  - und das zugehörige Kennwort.<br>
 *	- mehrere Nachrichtentexte können folgen.
 *
 *	Das Format der Parameterangabe ist:<br>
 *	2 Zeichen für den Parameter plus ':' (Doppelpunkt) und dann der Parameter<br>
 *	gültige Parameter sind:
 *	"MX:" (Mailserver), "US:" (User/Benutzer),
 *	"PW:" (Kennwort), "FR:" (From/Absender)
 *
 *	Nachrichten beginnen mit "##" und einer 2-stelligen Nummer der Nachricht,
 *	dahinter steht die Empfängeradresse. Die Nachricht enthält alle Zeilen
 *	bis zum Beginn der nächsten Nachricht ("##") und sollte maximal
 *	80 Zeichen pro Zeile enthalten. Die letzte Nachricht muß mit "####"
 *	terminiert werden. Der Nachrichtentext kann die gleichen Variablen
 *	enthalten, die auch in httpd möglich sind. (siehe \ref translate() )
 *
 *	Internet-Mailadressen (FR: und Empfängeradresse) müssen in spitzen Klammern \<\>
 *	eingeschlossen werden. Die Adresse des Mailservers wird über DNS aufgelöst.
 *	Der Text der Nachricht sollte komplett mit richtigem Header und Body
 *	formatiert sein (siehe RFC 2822)
 *
 *	\b Beispiel einer mail.ini-Datei mit 2 Nachrichten:
 *	\include mail.ini
 *
 *	Die erste mail (##01\<wil\@beispiel.de\>) ist als Beispiel die
 *	"Hello World!" version für SMTP aus den RFC.
 *	In Mail ##2 sind ein paar feste Werte durch Variablen ausgetauscht,
 *	die beim Versand ersetzt werden.
 *
 *	\see command_mail
 *
 *	Die Aktivierung erfolgt mittels #USE_MAIL in der config.h
 * @{
 */

/**
 * \file
 * sendmail
 *
 * \author W.Wallucks
 */

/**
 * \addtogroup mailintern	interne Hilfsfunktionen
*/
/**
 * @}
 */

/*
*  typedef und define
*/
/**
 * \ingroup mail
 * Zusammenfassung der für sendmail relevanten Parameter
 */
typedef struct
{
	uint8_t		aktiv	: 1;	//!< E-Mail Versand ist aktiv
	uint8_t		status;			//!< interner sendmail Status
	uint8_t		mailno;			//!< # der zu sendenden E-Mail
	uint32_t 	server_ip;		//!< IP des SMTP-Servers
	uint16_t	client_port;	//!< mein eigener TCP Port
	uint32_t	timeout;		//!< Timeout Zähler für DNS-Anfrage
} SENDMAIL_STATUS;

#define MX_WAIT_FOR_220		10	// initial Greeting

/*
**	lokale Variable
*/
PROGMEM char SMTP_HELO[] = "HELO ETH_M32_EX\r\n";
PROGMEM char SMTP_AUTH[] = "AUTH LOGIN\r\n";
PROGMEM char SMTP_RSET[] = "RSET\r\n";
PROGMEM char SMTP_DATA[] = "DATA\r\n";
PROGMEM char SMTP_END[]  = ".\r\n";
PROGMEM char SMTP_QUIT[] = "QUIT\r\n";

SENDMAIL_STATUS sm_status;		//!< Parameter für sendmail

/*
*  Prototypen
*/
void sendmail_data(unsigned char);
char *read_mailparam(const char *, char *, uint16_t);
uint16_t read_maildata(uint8_t);

//----------------------------------------------------------------------------
/**
 *	\ingroup mail
 *	Initialisierung des sendmail clients
 */
void sendmail_init (void)
{
	sm_status.aktiv			= 0;
	sm_status.status		= 0;
	sm_status.server_ip		= 0;
    sm_status.client_port 	= 63210;
	sm_status.timeout		= -1;

	add_tcp_app(sm_status.client_port, sendmail_data);
}

//----------------------------------------------------------------------------
/**
*	\ingroup mailintern
*	Response vom SMTP-Server erhalten
*/
void sendmail_data(uint8_t index)
{
	MAIL_DEBUG("\r\nsendmail_data %i",index);
    if (tcp_entry[index].status & FIN_FLAG) {	// Verbindungsabbau
		sm_status.aktiv = 0;
        return;
    }

	char *ptr = (char *)&eth_buffer[TCP_DATA_START_VAR];
	uint16_t status;

	if ( isdigit(*ptr) && isdigit(*(ptr+1)) && isdigit(*(ptr+2)) ) {
		status = atoi(ptr);
		MAIL_DEBUG("\r\nStatus-Code %i %s",status,ptr);
	}
	else
		return;	// kein Statuscode in Antwort -> verwerfen

	if (sm_status.status == 12 && status == 503) {
		// die Kiste will kein login -> bei MAIL FROM weitermachen
		status = 235;
		sm_status.status = 14;
	}

    if (sm_status.status != 19 && (status >= 500 || status == 451) ) {

	    MAIL_DEBUG("\r\n\r\n*** Error: Mail wurde nicht versendet ***\r\n");
        MAIL_DEBUG("\r\nStatus-Code: %i",status);
		// TODO: eventuell trotzdem noch QUIT senden ?
        sm_status.status = 0;
		sm_status.aktiv = 0;
        return;
    }

	tcp_entry[index].status = ACK_FLAG | PSH_FLAG;

	switch (sm_status.status) {

        case MX_WAIT_FOR_220:	// auf erste Meldung des SMTP-Servers warten
            if (status == 220)
            {
                MAIL_DEBUG("0: SMTP_HELO\n\r");
                memcpy_P(ptr,SMTP_HELO,sizeof(SMTP_HELO));		
                create_new_tcp_packet(sizeof(SMTP_HELO)-1,index);
                sm_status.status++;  
            }
            break;

        case 11:	// 
            if (status == 250)
            {
                MAIL_DEBUG("1: SMTP_AUTH\n\r");
                memcpy_P(ptr,SMTP_AUTH,sizeof(SMTP_AUTH));
					
                create_new_tcp_packet(sizeof(SMTP_AUTH)-1,index);  
                sm_status.status++;  
            }
            break; 
            
        case 12:
            if (status == 334)
            {
				char zeile[81];

                MAIL_DEBUG("2: Send Username: ");
                decode_base64( (unsigned char *)read_mailparam(PSTR("US"),zeile,80),(unsigned char *)ptr);
				strcat_P(ptr,PSTR("\r\n"));
                MAIL_DEBUG("%s",ptr);
                
                create_new_tcp_packet(strlen(ptr),index);   
                sm_status.status++;  
            }
            break;
            
        case 13:
            if (status == 334)
            {
				char zeile[81];

                MAIL_DEBUG("3: Send Password: ");
                decode_base64( (unsigned char *)read_mailparam(PSTR("PW"),zeile,80),(unsigned char *)ptr);
				strcat_P(ptr,PSTR("\r\n"));
                MAIL_DEBUG("%s",ptr);
                
                create_new_tcp_packet(strlen(ptr),index);   
                sm_status.status++;
            }
            break;
            
        case 14:
            if (status == 235)
            {
				char zeile[81];

                MAIL_DEBUG("4: SMTP_MAIL_FROM\n\r");
                strcpy_P(ptr,PSTR("MAIL FROM:"));
				strcat(ptr,read_mailparam(PSTR("FR"),zeile,80));
				strcat_P(ptr,PSTR("\r\n"));

                create_new_tcp_packet(strlen(ptr),index);  
                sm_status.status++;
            }
            break;
         
        case 15:
            if (status == 250)
            {
				char zeile[81];

                MAIL_DEBUG("5: SMTP_MAIL_RCPT_TO\n\r");
                strcpy_P(ptr,PSTR("RCPT TO:"));
				strcat(ptr,read_mailparam(PSTR("##"),zeile,80));
				strcat_P(ptr,PSTR("\r\n"));

                create_new_tcp_packet(strlen(ptr),index);
                sm_status.status++;
            }
            break;
            
        case 16:
            if (status == 250)
            {
                MAIL_DEBUG("6: SMTP_MAIL_DATA\n\r");
                memcpy_P(ptr,SMTP_DATA,sizeof(SMTP_DATA));

                create_new_tcp_packet(sizeof(SMTP_DATA)-1,index); 
                sm_status.status++;
            }
            break;
            
        case 17:
            MAIL_DEBUG("7: SMTP_MAIL_TEXT\n\r");
			create_new_tcp_packet(read_maildata(sm_status.mailno),index);
			// TODO:
			// hier muss noch Code rein, falls die Mail nicht in ein
			// einziges Paket passt. Dann darf der status nicht
			// hochgezählt werden und read_maildata() muss dann bei
			// erneutem Aufruf den Rest der Mail liefern.
			sm_status.status++;
            MAIL_DEBUG("\r\nTEXT\r\n%s",&eth_buffer[TCP_DATA_START_VAR]);
            break;

        case 18:
            MAIL_DEBUG("8: SMTP_MAIL_END\n\r");
            memcpy_P(&eth_buffer[TCP_DATA_START_VAR],SMTP_END,sizeof(SMTP_END));	
            create_new_tcp_packet(sizeof(SMTP_END)-1,index);  
            sm_status.status++;
            break;
            
        case 19:
            //if (status == 250)
            {
                MAIL_DEBUG("9: SMTP_MAIL_QUIT\n\r");
                memcpy_P(&eth_buffer[TCP_DATA_START_VAR],SMTP_QUIT,sizeof(SMTP_QUIT));	
                create_new_tcp_packet(sizeof(SMTP_QUIT)-1,index);  
                sm_status.status++;
            }
            break;
            
        case 20:
            MAIL_DEBUG("\r\n20: Mail wurde versendet!!\r\n");
			sm_status.aktiv = 0;
			sm_status.status = 0;
			break;
	}

	tcp_entry[index].time = TCP_TIME_OFF;
}

//----------------------------------------------------------------------------
/**
 *	\ingroup mail
 *	E-Mail Versand starten
 *
 * \param[in] mailno Nummer der zu versendenden Mail aus Ini-Datei
 * \returns 0 bei erfolgreicher Mail Initialisierung<br>
 *	\b mailno falls bereits ein Mailversand aktiv und noch nicht
 *	beendet ist, oder die DNS-Auflösung des SMPT-Servers noch nicht
 *	erfolgreich war.
 */
uint8_t sendmail(uint8_t mailno)
{
	// DNS-Auflösung des SMTP-Servers
	if (!sm_status.server_ip) {
		if (sm_status.status == 1) {
			// wir warten auf die DNS Antwort
			if ( time < sm_status.timeout )
				return mailno;	// noch warten ...
		}

		char zeile[81];
		char *ptr;

		// SMTP Hostnamen aus Ini-Datei lesen
		if ( (ptr = read_mailparam(PSTR("MX"),zeile,sizeof(zeile))) )
			dns_request(ptr, &sm_status.server_ip);
		else
			return 0;	// kein SMTP-Server angegeben

		sm_status.status = 1;
		sm_status.timeout = time + 3;	// 3 sec. Timeout
		return mailno;
	}

	if (sm_status.aktiv) {		// E-Mail ist aktiv
		return mailno;			// warten bis vorherige E-Mail verschickt
	}

	sm_status.mailno = mailno;	// # der Nachricht merken

	// Absender Port zufällig auswählen
    unsigned int my_mail_cp_new = sm_status.client_port + time;
    if (my_mail_cp_new < 1000) my_mail_cp_new +=1000;
    change_port_tcp_app (sm_status.client_port, my_mail_cp_new);
    sm_status.client_port = my_mail_cp_new;

	// ARP Request senden
	// und Socket öffnen
	if (arp_request(sm_status.server_ip))
    {
        for(unsigned long a=0;a<2000000;a++){asm("nop");}; 	// das ist noch etwas "unschön", 
															// besser flag setzen und dann in mainloop pollen
        
        MAIL_DEBUG("\r\nMail empfang am Clientport (%u)",(uint32_t)sm_status.client_port);
        tcp_port_open (sm_status.server_ip,HTONS(SMTP_PORT),HTONS(sm_status.client_port));
		sm_status.status = MX_WAIT_FOR_220;
		sm_status.aktiv = 1;	// Versand ist aktiv
    }

	return 0;	// E-Mail Versand ist gestartet -> diese Routine nicht mehr aufrufen
}

//----------------------------------------------------------------------------
/**
 *	\ingroup mailintern
 *	einzelnen Parameter aus Mail-Ini-Datei lesen
*/
char *read_mailparam(const char *parm, char *buffer, uint16_t len)
{
	char *ptr = (char *)0;

	File *inifile = f16_open(MAIL_DATAFILE,"r");
	if (!inifile)
		return ptr;

	while ( f16_gets(buffer, len, inifile) ) {
		MAIL_DEBUG("\r\n<|%s|>",buffer);

		if (!strncmp_P(buffer,parm,2)) {
			if (*buffer == '#') {	// E-Mail no.
				if ( sm_status.mailno == atoi(&buffer[2]) ) {
					ptr = buffer + 4;
					break;
				}
			}
			else {		// normaler Parameter
				ptr = buffer + 3;
				break;
			}
		}
	}

	f16_close(inifile);
	if (*(ptr+strlen(ptr)-2) == '\r')	// CRLF abschneiden
		*(ptr+strlen(ptr)-2) = 0;
	return ptr;
}

//----------------------------------------------------------------------------
/**
 *	\ingroup mailintern
 *	Daten für E-Mail einlesen
*/
uint16_t read_maildata(uint8_t mailno)
{
	char *ptr = (char *)&eth_buffer[TCP_DATA_START_VAR];
	uint16_t len = 0;
	uint16_t srclen;
	char buffer[81];

	File *inifile = f16_open(MAIL_DATAFILE,"r");
	if (!inifile)
		return 0;

	while ( f16_gets(buffer, sizeof(buffer), inifile) ) {
		MAIL_DEBUG("\r\n<|%s|>",buffer);

		if (!strncmp_P(buffer,PSTR("##"),2)) {
			if ( mailno == atoi(&buffer[2]) )
				break;
		}
	}

	while ( f16_gets(buffer, sizeof(buffer), inifile) && len < (MTU_SIZE-(TCP_DATA_START)-100) ) {
		MAIL_DEBUG("\r\n>|%s|<",buffer);

		if (!strncmp_P(buffer,PSTR("##"),2)) {
			break;	// nächste Mail beginnt
		}
		len += translate(buffer, &ptr, &srclen);
	}

	f16_close(inifile);
	return len;
}

//----------------------------------------------------------------------------
#endif //USE_MAIL





