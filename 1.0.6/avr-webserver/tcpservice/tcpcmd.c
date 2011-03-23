/*------------------------------------------------------------------------------
 Copyright:      Radig Ulrich + Wilfried Wallucks
 Author:         Radig Ulrich + W.Wallucks
 Remarks:        
 known Problems: none
 Version:        14.05.2008
 Description:    Command Interpreter

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

//----------------------------------------------------------------------------
#include "../config.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "tcpcmd.h"

#include "../usart.h"
#include "../stack.h"
#include "../timer.h"
#include "../networkcard/enc28j60.h"

#if USE_MMC
#include "../sdkarte/fat16.h"
#include "../sdkarte/sd_raw.h"
#include "../sdkarte/sdcard.h"
#include "../sdkarte/sd.h"
#endif

#if TCP_SERVICE
#include "tcpsrv.h"
#endif

#if USE_DNS
#include "../dns.h"
#endif

#if USE_NTP
#include "../ntp.h"
#endif

#if USE_MAIL
    extern unsigned char mail_enable;
#endif

#if USE_CAM
  #include "../camera/cam.h"
#endif

#if USE_WOL
  #include "../wol.h"
#endif

#if USE_OW
  #include "../messung.h"
  #include "../1-wire/onewire.h"
  #include "../1-wire/ds18x20.h"
#endif

/**
 * \addtogroup tcpsrv	TCP-Service für Telnet CMD-Line und FTP
 *
 * @{
  */

/**
 * \file
 * CMD Interpreter
 *
 * \author W.Wallucks & Ulrich Radig
 */

/**
 * \addtogroup tcpcmdhelper Hilfs-Funktionen für CMD-Interpreter
 *	Hilfs-Funktionen für CMD-Interpreter
 */

/**
 * \addtogroup tcpcmd Befehls-Funktionen
 *	Funktionen um Befehle
 *	von USART und über TCP auszuführen
 *
 * Dieses Modul enthält alle Befehle und die zugehörigen Funktionen, die
 * über USART oder über TCP ausgeführt werden können. Aufgerufen werden die
 * Funktionen über eine Tabelle <tt>CMD_TABLE[]</tt>, in der die Befehle (Textstrings)
 * und die zugehörigen Funktionspointer stehen.
 *
 * Die Hierarchie der Funktionen ist so, dass grundsätzlich alle über alle Schnittstellen
 * aufgerufen werden können. Einige Funktionen geben auf der USART nichsts aus, andere
 * geben nur auf der USART aus. Daher kann man die Funktionen grob wie folgt einteilen:
 * - Ausgabe nur auf USART	\see usartcmdline
 * - funktioniert auf USART und über TCP
 * 	- sinnvoll für FTP und USART
 * 	- sinnvoll nur für FTP / keine Ausgabe auf USART
 *
 * Um eine neue Funktion/Befehl zuzufügen sind folgende Schritte notwendig:
 * - Befehl (string) im Flash definieren: \code char p_neueFkt[] 	PROGMEM = "myCMD"; \endcode <br>
 * - Befehlsfunktion schreiben: 
 \code
 int16_t cmd_neueFkt(char *outbuffer)
 {
 // ...
 }
 \endcode 
 *	 dabei unterscheiden, ob outbuffer auf NULL gesetzt ist (Befehl über USART) oder nicht.<br>Bei TCP
 *	 muss das Ergebnis im outbuffer als Text gespeichert werden (zum Beispiel "200 OK") und die Länge des
 *	 Strings muss zurückgegeben werden.<br>
 *
 * - Zeiger auf Befehl und Funktion in Tabelle eintragen:<br>
 *	\code
	CMD_ITEM CMD_TABLE[] PROGMEM =
	{
	   {p_neueFkt,cmd_neueFkt},
	   // ...
 	\endcode
 * 	 
 * @{
 */

/**
 * \addtogroup tcpcmdcommon allgemeine CMD-Funktionen
 *	allgemeine Funktionen aufrufbar über Telnet oder USART
 */

/**
 * \addtogroup tcpftp FTP-Funktionen
 *	Funktionen primär für FTP
 */

/**
 * \addtogroup tcpcmdline Telnet CMD-Funktionen
 *	Funktionen primär für Telnet Kommandozeile
 */

/**
 * \addtogroup usartcmdline USART CMD-Funktionen
 *	Funktionen primär für USART Kommandozeile
 */

/**
 * @}
 * @}
 */

/*
**	lokale Variable
*/
char *argv;		//!< zeigt auf Parameterteil der Befehlszeile

#if TCP_SERVICE
extern TCPSRV_STATUS tcpsrv_status;
#endif

#define RESET() {asm("ldi r30,0"); asm("ldi r31,0"); asm("ijmp");}

/*
*  Prototypen
*/
void write_eeprom_ip(unsigned int);
uint8_t getLong(uint32_t *);
	
int16_t cmd_quit(char *);
int16_t command_reset(char *);
int16_t command_arp (char *);
int16_t command_tcp(char *);
int16_t command_ip(char *);
int16_t command_net(char *);
int16_t command_router(char *);
int16_t command_ntp(char *);
int16_t command_mac(char *);
int16_t command_ver(char *);
int16_t command_setvar(char *);
int16_t command_time(char *);
int16_t command_ntp_refresh	(char *);
int16_t command_ping(char *);
int16_t command_help(char *);
int16_t command_ADC(char *);

#if USE_WOL
int16_t command_wol(char *);
#endif

#if USE_MAIL
int16_t command_mail(char *);
#endif

#if USE_CAM
int16_t Bild_speichern(char *);
#endif

#if USE_DNS
int16_t cmd_DNS(char *);
int16_t cmd_DNSQ(char *);
int16_t cmd_DNSR(char *);
#endif

int16_t cmd_200(char *);
int16_t cmd_250(char *);
int16_t cmd_502(char *);
int16_t cmd_550(char *);

#if USE_MMC
int16_t print_disk_info(char *);
int16_t cat(char *);
int16_t cmd_PWD(char *);
int16_t cmd_CWD(char *);
int16_t cmd_CDUP(char *);
int16_t cmd_150(char *);
int16_t cmd_230(char *);
int16_t cmd_331(char *);
int16_t cmd_530(char *);
int16_t cmd_PASV(char *);
int16_t cmd_TYPE(char *);
int16_t cmd_RETR(char *);
int16_t cmd_LIST(char *);
int16_t cmd_STOR(char *);
int16_t cmd_DELE(char *);
int16_t cmd_RMD(char *);
int16_t cmd_MKD(char *);
int16_t cmd_SYST(char *);
int16_t cmd_USER(char *);
int16_t cmd_PASS(char *);
#endif

#if USE_OW
int16_t command_OWlookup(char *);
int16_t command_OWread(char *);
#endif

/*
 * \ingroup tcpcmd
 *  Texte der verwendete Befehle (im Flash gespeichert)
*/
char p_quit[] 	PROGMEM = "QUIT";
char p_exit[] 	PROGMEM = "EXIT";
char p_reset[] 	PROGMEM = "RESET";
char p_arp[]	PROGMEM = "ARP";
char p_tcp[]	PROGMEM = "TCP";
char p_ip[]		PROGMEM = "IP";
char p_net[] 	PROGMEM = "NET";
char p_router[]	PROGMEM = "ROUTER";
char p_ntp[] 	PROGMEM = "NTP";
char p_mac[] 	PROGMEM = "MAC";
char p_ver[] 	PROGMEM = "VER";
char p_sv[]	 	PROGMEM = "SV";
char p_time[]	PROGMEM = "TIME";
char p_ntpr[]	PROGMEM = "NTPR";
char p_ping[]	PROGMEM = "PING";

#if USE_WOL
char p_wol[] 	PROGMEM = "WOL";
#endif

#if USE_CAM
char p_cam[] 	PROGMEM = "BILD";
#endif

#if USE_DNS
char p_dns[] 	PROGMEM = "DNS";
char p_dnsqry[]	PROGMEM = "DNSQ";
char p_dnsrev[]	PROGMEM = "DNSR";
#endif

#if USE_MMC
char p_dir[] 	PROGMEM = "DIR";
char p_disk[] 	PROGMEM = "DISK";
char p_cat[] 	PROGMEM = "CAT";
char p_pwd[] 	PROGMEM = "PWD";
char p_cwd[] 	PROGMEM = "CWD";
char p_cd[]	 	PROGMEM = "CD";
char p_cdup[] 	PROGMEM = "CDUP";
char p_user[]	PROGMEM = "USER";
char p_pass[]	PROGMEM = "PASS";
char p_noop[] 	PROGMEM = "NOOP";
char p_type[] 	PROGMEM = "TYPE";
char p_pasv[] 	PROGMEM = "PASV";
char p_list[] 	PROGMEM = "LIST";
char p_retr[] 	PROGMEM = "RETR";
char p_stor[] 	PROGMEM = "STOR";
char p_rmd[] 	PROGMEM = "RMD";
char p_mkd[]	PROGMEM = "MKD";
char p_dele[] 	PROGMEM = "DELE";
char p_syst[] 	PROGMEM = "SYST";
#endif

#if USE_MAIL
char p_mail[] 	PROGMEM = "mail";
#endif

#if HELPTEXT
char p_help[] 	PROGMEM = "HELP";
char p_help2[] 	PROGMEM = "?";
#endif

#if USE_ADC
char p_adc[] 	PROGMEM = "ADC";
#endif

#if USE_OW
char p_ow[]	 	PROGMEM = "OW";
char p_owread[]	PROGMEM = "OWREAD";
#endif


/**
 * \ingroup tcpcmd
 *	Befehls-Tabelle (im Flash gespeichert)
 *
 *	Dies ist die eigentliche Befehlstabelle mit Zeigern auf die
 *	im Flash abgelegten Befehlsstrings und den zugehoerigen
 *	Funktionspointern
 */
CMD_ITEM CMD_TABLE[] PROGMEM =
{
	{p_quit,cmd_quit},				//!< Quit
	{p_exit,cmd_quit},				//!< Exit
	{p_reset,command_reset},		//!< Reset
	{p_arp,command_arp},			//!< Arp
	{p_tcp,command_tcp},			//!< Tcp
	{p_ip,command_ip},				//!< ip [aa.bb.cc.dd]
	{p_net,command_net},			//!< net [aa.bb.cc.dd] - Netzmaske
	{p_router,command_router},		//!< router [aa.bb.cc.dd]
	{p_mac,command_mac},			//!< mac [aa bb cc dd ee ff] - MAC-Adresse
	{p_ver,command_ver},
	{p_sv,command_setvar},
	{p_time,command_time},
	{p_ping, command_ping},

	{p_ntp,command_ntp},			//!< ntp [aa.bb.cc.dd] - Zeitserver
	{p_ntpr,command_ntp_refresh},
		
#if USE_WOL
	{p_wol,command_wol},
#endif

#if USE_CAM
	{p_cam,Bild_speichern},
#endif

#if USE_DNS
	{p_dns,cmd_DNS},
	{p_dnsqry,cmd_DNSQ},
	{p_dnsrev,cmd_DNSR},
#endif

#if USE_MMC
	{p_dir,cmd_MMCdir},
	{p_disk,print_disk_info},
	{p_cat,cat},
	{p_pwd,cmd_PWD},
	{p_cwd,cmd_CWD},
	{p_cd,cmd_CWD},
	{p_cdup,cmd_CDUP},
	{p_user,cmd_USER},
	{p_pass,cmd_PASS},
	{p_noop,cmd_200},
	{p_type,cmd_TYPE},
	{p_pasv,cmd_PASV},
	{p_list,cmd_LIST},
	{p_retr,cmd_RETR},
	{p_stor,cmd_STOR},
	{p_dele,cmd_DELE},
	{p_mkd,cmd_MKD},
	{p_rmd,cmd_DELE},
	{p_syst,cmd_SYST},
#endif

#if USE_MAIL
    {p_mail, command_mail},
#endif //USE_MAIL

#if HELPTEXT
	{p_help,command_help},
	{p_help2,command_help},
#endif

#if USE_ADC
	{p_adc,command_ADC},
#endif

#if USE_OW
	{p_ow,command_OWlookup},
	{p_owread,command_OWread},
#endif

	{NULL,NULL} 
};

#if USE_RC5 || DOXYGEN
/**
 * \ingroup rc5
 *	Befehls-Tabelle (in tcpcmd.c)
 *
 *	Dies ist die Tabelle mit Funktionspointern
 *	auf die per IR-Steuerung ausgeführten Kommandos
 */
cmd_fp RC5_TABLE[] PROGMEM = {
	command_reset,
	command_arp,
	command_tcp,
	command_ip,
	command_time,
	command_ntp_refresh,
		
#if USE_OW
	command_OWlookup,
	command_OWread,
#endif

#if USE_MMC
	cmd_MMCdir,
	print_disk_info,
#endif

#if USE_WOL
	command_wol,
#endif

#if USE_CAM
	Bild_speichern,
#endif

#if HELPTEXT
	command_help,
#endif
};
uint8_t RC5maxcmd = sizeof(RC5_TABLE)/sizeof(cmd_fp);
#endif

#if HELPTEXT
	PROGMEM char helptext[] = {
		"RESET  - reset the AVR - Controller\r\n"
		"ARP    - list the ARP table\r\n"
		"TCP    - list the tcp table\r\n"
		"IP     - list/change ip\r\n"
		"NET    - list/change netmask\r\n"
		"ROUTER - list/change router ip\r\n"
		"NTP    - list/change NTP\r\n"
		"NTPR   - NTP Refresh\r\n"
		"MAC    - list MAC-address\r\n"
		"VER    - list enc version number\r\n"
		"SV     - set variable\r\n"
        "PING   - send Ping\r\n"

        #if USE_MAIL
        "MAIL   - send E-Mail\r\n"
        #endif //USE_MAIL

		#if USE_WOL
		"WOL    - send WOL / set MAC / set MAC and IP\r\n"
		#endif

		#if USE_MMC
		"disk   - SD-Karteninformation\r\n"
		"dir    - Directory anzeigen\r\n"
		"cat    - Datei anzeigen\r\n"
		"cwd    - change working directory\r\n"
		"cd     - wie cwd\r\n"
		#endif

		#if USE_OW
		"OW		- 1-wire Sensoren suchen\r\n"
		"OWREAD	- vorbelegte 1-wire Sensoren auslesen\r\n"
		#endif

		"TIME   - get time\r\n"
		"HELP   - print Helptext\r\n"
		"?      - wie HELP\r\n"
	};
#endif

//------------------------------------------------------------------------------
/**
 * \ingroup tcpsrv
 * Tastatureingabe bei USART auswerten
 *
 * Die Funktion zerlegt den übergebenen Tastaturpuffer
 * in Befehl und Parameterliste. Dann wird die Befehlstabelle
 * <tt>CMD_TABLE[]</tt> nach dem Befehl durchsucht und die
 * passende Funktion aufgerufen.
 *
 * \param[in] pstr char-Pointer auf Eingabe-Puffer
 * \returns 0 bei Fehler, 1 bei ausgefuehrtem Befehl
 */
unsigned char extract_cmd(char *pstr)
{
	char *string_pointer_tmp;
	unsigned char cmd_index = 0;
 
 	// erst mal Tastaturpuffer in Kommando und Parameter aufteilen
    string_pointer_tmp = strsep(&pstr," "); 
	argv = pstr;	// Zeiger auf Cmd-Parameter setzen

	// Kommando in Tabelle CMD_TABLE suchen
	while(strcasecmp_P(string_pointer_tmp,(char *)pgm_read_word(&CMD_TABLE[cmd_index].cmd)))
    {
        // wenn letzter Eintrag ({NULL,NULL}) erreicht wurde,
		// Abruch der Whileschleife und Unterprogramm verlassen 
        if (pgm_read_word(&CMD_TABLE[++cmd_index].cmd) == 0) return(0);
    }
    
	// der Befehl wurde in der Tabelle gefunden;
	// jetzt Befehl ausführen und Ergebnis zurückgeben
	usart_write("\r\n");
	((cmd_fp)pgm_read_word(&CMD_TABLE[cmd_index].fp))(0);
	return(1); 
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdhelper
 * nächsten Wert als long von CMD-Parameter einlesen
 *
 * \param[out] pstr uint32_t(long)-Pointer für Ergebniswert
 * \returns 0 bei Fehler (kein(e) weiteren Werte vorhanden),<br>
 *  1 bei erfolgreicher Konversion
 */
uint8_t getLong(uint32_t *pstr)
{
	char *ps = strsep(&argv,"., ");

	if (ps) {
		*pstr = strtoul(ps,NULL,0);
		return 1;
	}
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmd
 * \b EXIT/QUIT-Befehl
 *
 * \returns (-1)
 */
int16_t cmd_quit(char *outbuffer)
{
	return (-1);
}
//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b Reset-Befehl ausführen
 */
int16_t command_reset (char *outbuffer)
{
	RESET();
	return 0;	// never executed
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b IP-Befehl print/edit own IP
 */
int16_t command_ip (char *outbuffer)
{
	write_eeprom_ip(IP_EEPROM_STORE);
	(*((unsigned long*)&myip[0])) = get_eeprom_value(IP_EEPROM_STORE,MYIP);
	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("My IP: %i.%i.%i.%i\r\n"),myip[0],myip[1],myip[2],myip[3]);
		return strlen(outbuffer);
	}
	else {
		usart_write("My IP: %1i.%1i.%1i.%1i\r\n",myip[0],myip[1],myip[2],myip[3]);
		return 0;
	}
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdhelper
 * IP aus CMD-Parameter im EEPROM speichern
 *
 * \param[in] eeprom_adresse Speicheradresse im EEPROM
 */
void write_eeprom_ip (unsigned int eeprom_adresse)
{
	uint32_t var;

	if (getLong(&var))
	{
		//value ins EEPROM schreiben
		for (unsigned char count = 0; count<4;count++)
		{
			eeprom_busy_wait ();
			eeprom_write_byte((unsigned char *)(eeprom_adresse + count),(uint8_t)var);
			if (!getLong(&var))
				break;
		}
	}
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b NTP-Befehl print/edit NTP Server IP
 */
int16_t command_ntp (char *outbuffer)
{
	#if USE_NTP
	write_eeprom_ip(NTP_IP_EEPROM_STORE);
	(*((unsigned long*)&ntp_server_ip[0])) = get_eeprom_value(NTP_IP_EEPROM_STORE,NTP_IP);
	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("NTP_Server: %i.%i.%i.%i\r\n"),ntp_server_ip[0],ntp_server_ip[1],ntp_server_ip[2],ntp_server_ip[3]);
		return strlen(outbuffer);
	}
	else {
		usart_write("NTP_Server: %1i.%1i.%1i.%1i\r\n",ntp_server_ip[0],ntp_server_ip[1],ntp_server_ip[2],ntp_server_ip[3]);
		return 0;
	}
	#endif //USE_NTP
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b Net-Befehl print/edit Netmask
 */
int16_t command_net (char *outbuffer)
{
	write_eeprom_ip(NETMASK_EEPROM_STORE);
	(*((unsigned long*)&netmask[0])) = get_eeprom_value(NETMASK_EEPROM_STORE,NETMASK);
	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("NETMASK: %i.%i.%i.%i\r\n"),netmask[0],netmask[1],netmask[2],netmask[3]);
		return strlen(outbuffer);
	}
	else {
		usart_write("NETMASK: %1i.%1i.%1i.%1i\r\n",netmask[0],netmask[1],netmask[2],netmask[3]);
		return 0;
	}
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b Router-Befehl print/edit Router IP
 */
int16_t command_router (char *outbuffer)
{
	write_eeprom_ip(ROUTER_IP_EEPROM_STORE);
	(*((unsigned long*)&router_ip[0])) = get_eeprom_value(ROUTER_IP_EEPROM_STORE,ROUTER_IP);
	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("250 ok. Router IP: %i.%i.%i.%i\r\n"),router_ip[0],router_ip[1],router_ip[2],router_ip[3]);
		return strlen(outbuffer);
	}
	else {
		usart_write("Router IP: %1i.%1i.%1i.%1i\r\n",router_ip[0],router_ip[1],router_ip[2],router_ip[3]);
		return 0;
	}
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b MAC-Befehl zeige eigene MAC-Adresse
 */
int16_t command_mac (char *outbuffer)
{
	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("250 ok. My MAC: %2x:%2x:%2x:%2x:%2x:%2x\r\n"),mymac[0],mymac[1],mymac[2],mymac[3],mymac[4],mymac[5]);
		return strlen(outbuffer);
	}
	else {
		usart_write("My MAC: %2x:%2x:%2x:%2x:%2x:%2x\r\n",mymac[0],mymac[1],mymac[2],mymac[3],mymac[4],mymac[5]);
		return 0;
	}
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b Ver-Befehl zeige enc28j60 chip version
 */
int16_t command_ver (char *outbuffer)
{
#if USE_ENC28J60
	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("250 ok. ENC28J60-Version: %1x\r\n"), enc28j60_revision);
		return strlen(outbuffer);
	}
	else {
		usart_write("ENC28J60-Version: %1x\r\n", enc28j60_revision);
		return 0;
	}
#endif

#if USE_RTL8019
	usart_write("RTL8019 Ethernetcard\r\n");
#endif
}

//------------------------------------------------------------------------------
extern struct arp_table arp_entry[MAX_ARP_ENTRY];
/**
 * \ingroup usartcmdline
 * \b arp-Befehl zeige ARP-Tabelle
 */
int16_t command_arp (char *outbuffer)
{
	if (outbuffer)					// momentan nur bei USART
		return cmd_502(outbuffer);

	for (unsigned char index = 0;index<MAX_ARP_ENTRY;index++)
	{
		usart_write("%2i  MAC:%2x",index,arp_entry[index].arp_t_mac[0]);
		usart_write(".%2x",arp_entry[index].arp_t_mac[1]);
		usart_write(".%2x",arp_entry[index].arp_t_mac[2]);
		usart_write(".%2x",arp_entry[index].arp_t_mac[3]);
		usart_write(".%2x",arp_entry[index].arp_t_mac[4]);
		usart_write(".%2x",arp_entry[index].arp_t_mac[5]);
		
		usart_write("  IP:%3i",(arp_entry[index].arp_t_ip&0x000000FF));
		usart_write(".%3i",((arp_entry[index].arp_t_ip&0x0000FF00)>>8));
		usart_write(".%3i",((arp_entry[index].arp_t_ip&0x00FF0000)>>16));
		usart_write(".%3i",((arp_entry[index].arp_t_ip&0xFF000000)>>24));
			
		usart_write("  Time:%4i\r\n",arp_entry[index].arp_t_time);
	}
	return 0;
}

//------------------------------------------------------------------------------
extern struct tcp_table tcp_entry[MAX_TCP_ENTRY+1];

/**
 * \ingroup usartcmdline
 * \b TCP-Befehl zeige TCP-Tabelle
 */
int16_t command_tcp (char *outbuffer)
{
	if (outbuffer)					// momentan nur bei USART
		return cmd_502(outbuffer);

	for (unsigned char index = 0;index<MAX_TCP_ENTRY;index++)
	{
		usart_write("%2i",index);
		usart_write("  IP:%3i",(tcp_entry[index].ip&0x000000FF));
		usart_write(".%3i",((tcp_entry[index].ip&0x0000FF00)>>8));
		usart_write(".%3i",((tcp_entry[index].ip&0x00FF0000)>>16));
		usart_write(".%3i",((tcp_entry[index].ip&0xFF000000)>>24));
		usart_write(" SRC_PORT:%4i",HTONS(tcp_entry[index].src_port));
		usart_write(" DEST_PORT:%4i",HTONS(tcp_entry[index].dest_port));
		usart_write(" Time:%4i\r\n",tcp_entry[index].time);
	}
	return 0;
}


//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b VAR-Befehl: ändern/anzeigen einer unsigned int Variable
 *
 * \b Syntax VAR \<index\> [\<Wert\>]<br>
 * \<index\>	 Speicherplatz der Variable<br>
 * \<Wert\>		 optional neuer Wert oder -1 zur Anzeige des aktuellen Wertes
 *
 * \attention Die Speicherplätze 0 bis 4 werden auch vom AD-Wandler verwendet
 * und somit durch die aktuellen ADC-Werte überschrieben.
 */
int16_t command_setvar (char *outbuffer)
{
#if USE_ADC
	uint8_t i;
	uint32_t var = 0;

	getLong(&var);
	i = (uint8_t) var;
	getLong(&var);
	if( var <= UINT16_MAX ) {	// falls größer 64K (oder -1) wird die Variable nur angezeigt
		var_array[i] = (unsigned int)var;
	}

	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("Inhalt der Variable[%2i] = %2u\r\n"),i,var_array[i]);
		return strlen(outbuffer);
	}
	else {
		usart_write("Inhalt der Variable[%2i] = %2u\r\n",i,(uint32_t)var_array[i]);
	}
#endif
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * TIME-Befehl: aktuelle Uhrzeit des Moduls ausgeben
 */
int16_t command_time (char *outbuffer)
{
	unsigned char hh = (time/3600)%24;
	unsigned char mm = (time/60)%60;
	unsigned char ss = time %60;
	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("250 ok. Time: %2i:%2i:%2i\r\n"),hh,mm,ss);
		return strlen(outbuffer);
	}
	else {
		usart_write ("\n\rTIME: %2i:%2i:%2i\r\n",hh,mm,ss);
		return 0;
	}
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b NTPR-Befehl Time Refresh via NTP-Server
 */
int16_t command_ntp_refresh (char *outbuffer)
{
	#if USE_NTP
	ntp_request();
	#endif //USE_NTP
	return cmd_250(outbuffer);
}

//------------------------------------------------------------------------------
#if USE_MAIL || DOXYGEN
/**
 * \ingroup tcpcmdcommon
 * \b MAIL-Befehl Sendet eine vorgefertigte E-MAIL
 *
 *	\b Syntax: mail \<mailno\> <br>
 *	\<mailno\> ist dabei die Nummer einer vorhandenen Mail in mail.ini
 */
int16_t command_mail (char *outbuffer)
{
	machineStatus.sendmail = atoi(argv);
	return cmd_250(outbuffer);
}
#endif //USE_MAIL

//------------------------------------------------------------------------------
#if USE_CAM || DOXYGEN
/**
 * \ingroup tcpcmdcommon
 * \b Kamerabild auf SD-Karte speichern
 *
 *	\b Syntax: bild<br>
 *
 *	\author Fred Fröhlich
 */
int16_t Bild_speichern(char *outbuffer)
{
	#if USE_MMC		// nur sinnvoll mit SD-Karte
	unsigned long apos;
	uint16_t a;
	char fname[13];

	max_bytes = cam_picture_store(CAM_RESOLUTION);      //Kamera läd neues Bild in Speicher

	// positive Antwort mit Dateiname in outbuffer speichern
	sprintf_P(fname,PSTR("%02i%02i%02i%02i.jpg"),TM_DD,TM_hh,TM_mm,TM_ss);
	File *picfile = f16_open(fname,"a");
	if (picfile) {
		apos = 0;
		do {
			a = 0;
			do {
				cam_data_get(apos);
				apos++;
				a++;
				if(a==512) break;
			} while(apos < max_bytes);
			fat16_write_file(picfile, (uint8_t *)&cam_dat_buffer[0], a);

		} while(apos < max_bytes);

		f16_close(picfile);

		if (outbuffer) {
			sprintf_P(outbuffer,PSTR("250 Datei: %s"),fname);
			return strlen(outbuffer);	// OK: 250 und Dateiname
		}
		else
			return 0;
	}
	else
	#endif
		return cmd_550(outbuffer);	// Fehler zurückgeben
}
#endif //USE_CAM

//------------------------------------------------------------------------------
/**
 * \ingroup usartcmdline
 * \b WOL-Befehl Wake-On-Lan Paket versenden
 */
int16_t command_wol (char *outbuffer)
{
	if (outbuffer)					// nur bei USART
		return cmd_502(outbuffer);

	#if USE_WOL
	uint32_t var = 0;

	// EEPROM beschreiben, falls Parameter angegeben wurden
	if (getLong(&var))
	{	
		//schreiben der MAC
		for (unsigned char count = 0; count<6; count++)
		{
			eeprom_busy_wait ();
			eeprom_write_byte((unsigned char *)(WOL_MAC_EEPROM_STORE + count),(uint8_t)var);
			if (!getLong(&var))
				break;
		}

		//zusätzlich schreiben der Broadcast-Adresse, falls vorhandenden
		if (getLong(&var))
		{
			for (unsigned char count = 0; count<4;++count)
			{
				eeprom_busy_wait ();
				eeprom_write_byte((unsigned char*)(WOL_BCAST_EEPROM_STORE + count),(uint8_t)var);
				if (!getLong(&var))
					break;
			}
		}

		//init
		wol_init();
	}else{
		//MagicPacket senden
		wol_request();
	}
	#endif //USE_WOL	
	return 0;
}


#if USE_DNS || DOXYGEN
//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b DNS-Befehl IP des DNS-Server anzeigen/setzen
 *
 * \b Syntax: DNS [a.b.c.d]
 */
int16_t cmd_DNS(char *outbuffer)
{
	write_eeprom_ip(DNS_IP_EEPROM_STORE);
	(*((unsigned long*)&dns_server_ip[0])) = get_eeprom_value(DNS_IP_EEPROM_STORE,DNS_IP);
	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("250 ok. DNS IP: %i.%i.%i.%i\r\n"),dns_server_ip[0],dns_server_ip[1],dns_server_ip[2],dns_server_ip[3]);
		return strlen(outbuffer);
	}
	else {
		usart_write("DNS IP: %1i.%1i.%1i.%1i\r\n",dns_server_ip[0],dns_server_ip[1],dns_server_ip[2],dns_server_ip[3]);
		dns_reverse_request(IP(192,168,52,200),usart_rx_buffer);
		return 0;
	}
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b DNS-Query Befehl
 *
 * \b Syntax: DNSQ \<hostname\>
 */
int16_t cmd_DNSQ(char *outbuffer)
{
	dns_request(argv,&dnsQueryIP);

	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b DNS-Reverse-Lookup Befehl
 *
 * \b Syntax: DNSR \<ip\>
 */
int16_t cmd_DNSR(char *outbuffer)
{
	uint32_t ip = 0;
	uint32_t var;

	if (getLong(&var)) {

		for (unsigned char count=0; count<4;++count) {
			ip += ( var<<(count*8) );
			if (!getLong(&var))
				break;
		}
	}

	// überschreibt den IP-String in der Kommandozeile (usart_rx_buffer)
	// mit dem gefundenen hostnamen
	dns_reverse_request(ip,usart_rx_buffer);

	return 0;
}
#endif

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b SYST-Befehl Systembeschreibung für FTP
 *
 * \b Ausgabe:
 * - Falls UNIX_LIST definiert ist "215 UNIX"
 * - Falls DOS_LIST definiert ist  "215 DOS"
 */
int16_t cmd_SYST(char *outbuffer)
{
	if (outbuffer) {
#ifdef UNIX_LIST
		strcpy_P(outbuffer,PSTR("215 UNIX\r\n"));
#endif
#ifdef DOS_LIST
		strcpy_P(outbuffer,PSTR("215 DOS\r\n"));	//215 Windows_NT
#endif
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmdcommon
 * \b DIR-Befehl Verzeichnislisting
 *
 * Aufruf von USART direkt über DIR-Befehl. Von FTP wird die Funktion
 * paketweise vom Datenkanal aus aufgerufen.<br><br>
 * \b Ausgabe:
 * - auf USART wird komplettes Listing ausgegeben
 * - bei TCP wird der outbuffer mit vollständigen Verzeichniseinträgen
 *   bis maximal MAX_WINDOWS_SIZE gefüllt. Falls dann noch Einträge
 *   vorhanden sind wird <tt>tcpsrv_status.LISTcontinue</tt> auf true gesetzt.
 * 	 Bei abgeschlossenem Listing wird <tt>tcpsrv_status.LISTcontinue</tt> auf
 *   false gesetzt.
 */
int16_t cmd_MMCdir(char *outbuffer)
{
#if USE_MMC
	if (!cwdir_ptr)
		return 0;

	#if !FTP_ANONYMOUS
	if (outbuffer && !tcpsrv_status.loginOK)
		return cmd_530(outbuffer);
	#endif

	#if TCP_SERVICE
	tcpsrv_status.LISTcontinue = 0;	// continue Flag zurücksetzen
	#endif
	if (!outbuffer) {
		usart_write("\n\rSD-Karte:");
	}

	char *pstr = outbuffer;
    struct fat16_dir_entry_struct dir_entry;
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;

    /* Verzeichniseinträge lesen */
    while(fat16_read_dir(cwdir_ptr, &dir_entry))
    {
		fat16_get_file_modification_date(&dir_entry, &year, &month, &day);
		fat16_get_file_modification_time(&dir_entry, &hour, &min, &sec);

		if (outbuffer) {
			#if TCP_SERVICE
			char tmpbuf[12];

			#ifdef UNIX_LIST
			/*
			*  UNIX style Dateiliste
			*/
			char mstr[4];

			if (dir_entry.attributes & FAT16_ATTRIB_DIR) {
				strcpy_P((char *)tmpbuf,PSTR("drwxr-xr-x"));
			}
			else {
				strcpy_P((char *)tmpbuf,PSTR("-rw-rw-rw-"));
			}

			strncpy_P(mstr,&US_Monate[(month-1)*3],3);
			mstr[3] = '\0';
			sprintf_P((char *)pstr,PSTR("%s 1 ftp ftp %ld %s/%i/%i %i:%i %s\r\n"),tmpbuf, dir_entry.file_size,
																mstr, day, year, hour, min, dir_entry.long_name);
			#endif

			#ifdef DOS_LIST
			/*
			*  DOS style Dateiliste
			* (bei FileZilla nennt sich das DOS - woanders Windows_NT(?) ...)
			*/
			if (dir_entry.attributes & FAT16_ATTRIB_DIR) {
				strcpy_P((char *)tmpbuf,PSTR("<DIR>"));
			}
			else {
				sprintf_P(tmpbuf,PSTR("%ld"),dir_entry.file_size);
			}

			sprintf_P((char *)pstr,PSTR("%i-%i-%i %i:%i:%i %8s %s\r\n"),month, day, year, hour, min, sec, tmpbuf,dir_entry.long_name);
			#endif

			while (*pstr++);	// bis auf '\0' vorzählen
			--pstr;

			if (pstr > (outbuffer + MAX_WINDOWS_SIZE - 64)) {	// mind. 64 Bytes für nächsten Eintrag freilassen
				tcpsrv_status.LISTcontinue = 1;					// continue Flag setzen
				break;
			}
			#endif
		}
		else {
			usart_write("\r\n%2i.%2i.%i %2i:%2i ",day, month, year, hour, min);
			if (dir_entry.attributes & FAT16_ATTRIB_DIR)
				usart_write("    <DIR> ");
			else
				usart_write("%9l ",dir_entry.file_size);

			usart_write("%s",dir_entry.long_name);
		}
    }

	#if TCP_SERVICE
	if (!tcpsrv_status.LISTcontinue)
		fat16_reset_dir(cwdir_ptr);
	#endif

	if (outbuffer)
		return strlen(outbuffer);
	else
#endif
		return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup usartcmdline
 * \b CAT-Befehl Anzeige eines Dateiinhalts auf der USART
 */
int16_t cat(char *outbuffer)
{
	if (outbuffer)					// nur bei USART
		return cmd_502(outbuffer);

	#if USE_MMC
	int ch;

	File *ptrFile = f16_open(argv,"r");
	if (!ptrFile) return 0;

	ch = f16_getc(ptrFile);
	while ( ch > 0 ) {
		usart_write_char((char)ch);
		ch = f16_getc(ptrFile);
		}

	f16_close(ptrFile);
	#endif
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b RETR-Befehl Dateianforderung bei FTP
 *
 * initialisiert die Dateiübertragung im FTP-Datenkanal
 */
int16_t cmd_RETR(char *outbuffer)
{
	if (outbuffer) {
		#if TCP_SERVICE
		#if !FTP_ANONYMOUS
		if (!tcpsrv_status.loginOK)
			return cmd_530(outbuffer);
		#endif

		if( f16_exist(argv) ) {
			strcpy((char *)tcpsrv_status.fname,argv);
			tcpsrv_status.data_state = 2;
			// Anfang der Übertragung
			return cmd_150(outbuffer);
		}
		else
		#endif
			cmd_502(outbuffer);	// TODO richtige Fehlermeldung einsetzen
	}
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b LIST-Befehl Verzeichnisanzeige bei FTP
 *
 * initialisiert die Verzeichnisanzeige im FTP-Datenkanal
 */
int16_t cmd_LIST(char *outbuffer)
{
	if (outbuffer) {
	  #if TCP_SERVICE
		#if !FTP_ANONYMOUS
		if (!tcpsrv_status.loginOK)
			return cmd_530(outbuffer);
		#endif

		tcpsrv_status.data_state = 1;
		tcpsrv_status.LISTcontinue = 0; 	// reset Listing
		// Anfang der Übertragung
		return cmd_150(outbuffer);
	  #else
		return cmd_502(outbuffer);
	  #endif
	}
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b STOR-Befehl Datenempfang bei FTP
 *
 * initialisiert den Datenempfang im FTP-Datenkanal
 */
int16_t cmd_STOR(char *outbuffer)
{
	if (outbuffer) {
	  #if TCP_SERVICE
		#if !FTP_ANONYMOUS
		if (!tcpsrv_status.loginOK)
			return cmd_530(outbuffer);
		#endif

		if( f16_exist(argv) ) {
			// Datei löschen
			f16_delete(argv);
		}

		strcpy((char *)tcpsrv_status.fname,argv);
		tcpsrv_status.datafile = f16_open(argv,"a");	// Datendatei öffnen
		tcpsrv_status.data_state = 3;
		// Anfang der Übertragung
		return cmd_150(outbuffer);
	  #else
		return cmd_502(outbuffer);
	  #endif
	}
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b DELE-Befehl Datei-/Verzeichnis löschen
 *
 * Dateien und Verzeichniseinträge werden bei FAT16 identisch behandelt.
 */
int16_t cmd_DELE(char *outbuffer)
{
#if USE_MMC
	#if !FTP_ANONYMOUS
	if (outbuffer && !tcpsrv_status.loginOK)
		return cmd_530(outbuffer);
	#endif

	if( f16_exist(argv) ) {
		usart_write("\r\nDELE: %s exist.",argv);
		if (f16_delete(argv) && outbuffer)
				return cmd_200(outbuffer);	// success
	}
#endif

	if (outbuffer) {
		return cmd_550(outbuffer);
	}
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b MKD-Befehl Verzeichnis anlegen
 */
int16_t cmd_MKD(char *outbuffer)
{
#if USE_MMC
	#if !FTP_ANONYMOUS
	if (outbuffer && !tcpsrv_status.loginOK)
		return cmd_530(outbuffer);
	#endif

	if( f16_mkdir(argv) && outbuffer) {
		return cmd_200(outbuffer);	// success
	}
#endif

	if (outbuffer)
		return cmd_550(outbuffer);
	else
		return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b PWD-Befehl aktuelles Verzeichnis anzeigen
 */
int16_t cmd_PWD(char *outbuffer)
{
#if !USE_MMC
	return cmd_502(outbuffer);
#else
	#if !FTP_ANONYMOUS
	if (outbuffer && !tcpsrv_status.loginOK)
		return cmd_530(outbuffer);
	#endif

	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("257 \"%s\" is your current location\r\n"),cwdirectory);
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
#endif
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 *	\b CWD bzw. \b CD-Befehl
 *
 *	neuen Pfad in cwdirectory abspeichern und auf Existenz überprüfen
 *  \remarks
 *	cwdirectory enthält den aktuellen Pfad ohne trailing "/"
 *	nur das Rootverzeichnis enthält "/"
*/
int16_t cmd_CWD(char *outbuffer)
{
#if !USE_MMC
	return cmd_502(outbuffer);
#else
	#if !FTP_ANONYMOUS
	if (outbuffer && !tcpsrv_status.loginOK)
		return cmd_530(outbuffer);
	#endif

	char tmpdir[MAX_PATH+1];

	if (*argv != '/') {								// bei aktuellem directory beginnen

		strcpy(tmpdir,cwdirectory);

		if ((strlen(argv) == 2) && *argv == '.' && *(argv+1) == '.') {
			strcat_P(argv,PSTR("/"));
		}

		while ((strlen(argv) > 2) && *argv == '.' && *(argv+1) == '.' && *(argv+2) == '/') {
			// ein Verzeichnis nach oben
			int8_t i = strlen(tmpdir) - 1;

			for (;i>=0;--i) {
				if (tmpdir[i] == '/') {
					tmpdir[i] = '\0';
					break;
				}
			}

			if (i <= 0) {
				tmpdir[0] = '/';	// oben angekommen
				tmpdir[1] = '\0';
			}

			argv += 3;
		}

		if (*argv == '.' && *(argv+1) == '/') argv += 2;	// zeigt auf aktuelles Verzeichnis

		register uint8_t len = strlen(tmpdir);
		register uint8_t lennew = strlen(argv);

		if ( (len > 1) && lennew )			// falls nicht root und neues Unterverzeichnis übergeben wurde
			strcat_P(tmpdir,PSTR("/"));		// also nicht ".." eingegeben

		if ((len + lennew) < MAX_PATH) {	// falls der Pfad auch in den Speicherplatz passt

			register char *ptr = argv;		// letzen '/' bei Unterverzeichnis entfernen
			while (*ptr++);
			ptr -= 2;
			if (*ptr == '/') *ptr = '\0';

			strcat(tmpdir,argv);
		}
	}
	else {
		register uint8_t lennew = strlen(argv) - 1;		// letzten '/' entfernen falls vorhanden
		if ( lennew > 0 && *(argv + lennew) == '/' )
			*(argv + lennew) = '\0';
		strcpy(tmpdir,argv);
	}

	// Verzeichnis überprüfen und Zeiger neu setzen
    struct fat16_dir_entry_struct directory;
	if (fat16_get_dir_entry_of_path(sd_get_fs(), tmpdir, &directory)) {
		// neues Verzeichnis gefunden
		strcpy((char *)cwdirectory,tmpdir);
		struct fat16_dir_struct* dir_new = fat16_open_dir(sd_get_fs(), &directory);
		if(dir_new) {
			if(cwdir_ptr != sd_get_root_dir())
				fat16_close_dir(cwdir_ptr);
			cwdir_ptr = dir_new;
		}
	}

	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("250 Directory changed to %s\r\n"),cwdirectory);
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
#endif
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 *	\b CDUP-Befehl
 *
 *	ein Verzeichnis in der Hierarchie nach oben wechseln
*/
int16_t cmd_CDUP(char *outbuffer)
{
#if !USE_MMC
	return cmd_502(outbuffer);
#else
	#if !FTP_ANONYMOUS
	if (outbuffer && !tcpsrv_status.loginOK)
		return cmd_530(outbuffer);
	#endif

	// ein Verzeichnis nach oben
	int8_t i = strlen(cwdirectory) - 1;
	char *pdir = (char *)cwdirectory;

	for (;i>=0;--i) {
		if (*(pdir+i) == '/') {
			*(pdir+i) = '\0';
			break;
		}
	}

	if (i <= 0) {
		strcpy_P((char *)cwdirectory,PSTR("/"));	// oben angekommen
	}

	// Zeiger aktualisieren
    struct fat16_dir_entry_struct directory;
	if (fat16_get_dir_entry_of_path(sd_get_fs(), cwdirectory, &directory)) {
		struct fat16_dir_struct* dir_new = fat16_open_dir(sd_get_fs(), &directory);
		if(dir_new) {
			if(cwdir_ptr != sd_get_root_dir())
				fat16_close_dir(cwdir_ptr);
			cwdir_ptr = dir_new;
		}
	}

	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("250 Directory changed to %s\r\n"),cwdirectory);
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
#endif
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b TYPE-Befehl für FTP
 *
 * intern gibt es keinen Unterschied ob binäer oder als ASCII übertragen
 * wird, da wir auf einer 8-bit Maschine arbeiten und die Übertragung
 * auch 8-bitig abläuft. Also kein Grund für Konvertierungen.
 */
int16_t cmd_TYPE(char *outbuffer)
{
#if !TCP_SERVICE
	return cmd_502(outbuffer);
#else
	if (outbuffer) {
		if (*argv == 'I') {
			strcpy_P(outbuffer,PSTR("200 Using BINARY mode to transfer data.\r\n"));
			tcpsrv_status.transfermode = 1;
		}
		else {
			strcpy_P(outbuffer,PSTR("200 Using ASCII mode to transfer data.\r\n"));
			tcpsrv_status.transfermode = 0;
		}
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
#endif
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b USER-Befehl
 *
 * - bei aktiviertem anonymen Zugang über TCP wird "200 OK" zurückgegeben
 * - bei nicht anonymem Zugang wird mit "331 ..." Kennwort angefordert.
 * - bei USART passiert nichts.
 */
int16_t cmd_USER(char *outbuffer)
{
	if (outbuffer) {
		#if FTP_ANONYMOUS
		return cmd_200(outbuffer);

		#else
		if (strcmp_P(argv,PSTR(FTP_USER))) {
			// Fehler
			tcpsrv_status.userOK = 0;
		}
		else {
			tcpsrv_status.userOK = 1;
		}
		// DEBUG: usart_write("\r\nUSER: %s %i",argv,tcpsrv_status.userOK);
		// egal ob zulässig oder nicht, Fehler wird erst nach User/Password gesendet
		strcpy_P(outbuffer,PSTR("331 FTP login okay, send password.\r\n"));
		return strlen(outbuffer);
		#endif
	}

	return 0;	// bei USART
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b PASS-Befehl
 *
 * - bei aktiviertem anonymen Zugang über TCP wird "200 OK" zurückgegeben
 * - bei nicht anonymem Zugang wird die Kombination aus FTP_USER/FTP_PASSWORD
 * überprüft und entsprechend "230" oder "530" geantwortet und der Zugang
 * über Statusflags frei/gesperrt geschaltet.
 * - bei USART passiert nichts.
 */
int16_t cmd_PASS(char *outbuffer)
{
	if (outbuffer) {
		#if FTP_ANONYMOUS
		return cmd_200(outbuffer);

		#else
		if (tcpsrv_status.userOK && !strcmp_P(argv,PSTR(FTP_PASSWORD))) {
			// User/Kennwort OK
			strcpy_P(outbuffer,PSTR("230 Welcome.\r\n"));
			tcpsrv_status.loginOK = 1;
		}
		else {
			strcpy_P(outbuffer,PSTR("530 Login incorrect.\r\n"));
			tcpsrv_status.loginOK = 0;
		}
		// DEBUG: usart_write("\r\nPASS: %s %i",argv,tcpsrv_status.userOK);
		tcpsrv_status.userOK = 0;	// User zurücksetzen

		// egal ob zulässig oder nicht, Fehler wird erst nach User/Password gesendet
		return strlen(outbuffer);
		#endif
	}

	return 0;	// bei USART
}

//------------------------------------------------------------------------------
int16_t cmd_150(char *outbuffer)
{
	if (outbuffer) {
	  #if !TCP_SERVICE
		return cmd_502(outbuffer);
	  #else
		if (tcpsrv_status.transfermode)
			strcpy_P(outbuffer,PSTR("150 Opening BINARY mode data connection.\r\n"));
		else
			strcpy_P(outbuffer,PSTR("150 Opening ASCII mode data connection.\r\n"));
		return strlen(outbuffer);
	  #endif
	}
	else {
		return 0;
	}
}

int16_t cmd_200(char *outbuffer)
{
	if (outbuffer) {
		strcpy_P(outbuffer,PSTR("200 Command ok.\r\n"));
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
}

int16_t cmd_250(char *outbuffer)
{
	if (outbuffer) {
		strcpy_P(outbuffer,PSTR("250 ok.\r\n"));
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
}

int16_t cmd_502(char *outbuffer)
{
	if (outbuffer) {
		strcpy_P(outbuffer,PSTR("502 not implemented.\r\n"));
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
}

int16_t cmd_530(char *outbuffer)
{
	if (outbuffer) {
		strcpy_P(outbuffer,PSTR("530 Not logged in.\r\n"));
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
}

int16_t cmd_550(char *outbuffer)
{
	if (outbuffer) {
		strcpy_P(outbuffer,PSTR("550 Requested action not taken.\r\n"));
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpftp
 * \b PASV-Befehl
 *
 * passiver mode auf Port 2100 (8 * 256 + 52)
 */
int16_t cmd_PASV(char *outbuffer)
{
	if (outbuffer) {
		sprintf_P(outbuffer,PSTR("227 Entering Passive Mode (%i,%i,%i,%i,8,52)\r\n"),myip[0],myip[1],myip[2],myip[3]);
		return strlen(outbuffer);
	}
	else {
		return 0;
	}
}


//------------------------------------------------------------------------------
/**
 * \ingroup usartcmdline
 * \b ADC-Befehl analoge Werte der AD-Wandler anzeigen
 */
int16_t command_ADC(char *outbuffer)
{
	if (outbuffer)					// nur bei USART
		return cmd_502(outbuffer);

	#if USE_ADC
	int8_t	i;

	for (i=0; i<MAX_VAR_ARRAY; i++) {
		usart_write("\r\nADC-Kanal(%i) = %i",i,var_array[i]);
	}
	usart_write("\r\n");
	#endif
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup usartcmdline
 * \b OW-Befehl T-Werte der fixen DS18x20 anzeigen
 */
int16_t command_OWread(char *outbuffer)
{
	if (outbuffer)					// nur bei USART
		return cmd_502(outbuffer);

	#if USE_OW
	uint8_t i;

	lese_Temperatur();

	usart_write("\r\nFixed T-Sensor Werte:");
	for (i=0; i<MAXSENSORS; i++) {
		usart_write("\r\n%i: %i",(i+1),ow_array[i]);
	}

	usart_write("\r\n");
	#endif
	return 0;
}

//------------------------------------------------------------------------------
#define MAXLOOKUP	5		// max. Anzahl der zu suchenden Sensoren
/**
 * \ingroup usartcmdline
 * \b OWREAD-Befehl DS18x20 auf Bus suchen und anzeigen
 */
int16_t command_OWlookup(char *outbuffer)
{
	if (outbuffer)					// nur bei USART
		return cmd_502(outbuffer);

	#if USE_OW
	uint8_t i;
	uint8_t diff, nSens;
	uint16_t TWert;
	uint8_t subzero, cel, cel_frac_bits;
	uint8_t gSensorIDs[MAXLOOKUP][OW_ROMCODE_SIZE];
	
	usart_write("\r\nScanning Bus for DS18X20");
	
	nSens = 0;
	
	for( diff = OW_SEARCH_FIRST; 
		diff != OW_LAST_DEVICE && nSens < MAXLOOKUP ;  )
	{
		DS18X20_find_sensor( &diff, &gSensorIDs[nSens][0] );
		
		if( diff == OW_PRESENCE_ERR ) {
			usart_write("\r\nNo Sensor found");
			break;
		}
		
		if( diff == OW_DATA_ERR ) {
			usart_write("\r\nBus Error");
			break;
		}
		
		nSens++;
	}
	usart_write("\n\r%i 1-Wire Sensoren gefunden.\r\n", nSens);

//	for (i=0; i<nSens; i++) {
//		// set 10-bit Resolution - Alarm-low-T 0 - Alarm-high-T 85
//		DS18X20_write_scratchpad( &gSensorIDs[i][0] , 0, 85, DS18B20_12_BIT);
//	}

	for (i=0; i<nSens; i++) {
		usart_write("\r\n#%i ist ein ",(int) i+1);
		if ( gSensorIDs[i][0] == DS18S20_ID)
			usart_write("DS18S20/DS1820");
		else usart_write("DS18B20");

		usart_write(" mit ");
		if ( DS18X20_get_power_status( &gSensorIDs[i][0] ) ==
			DS18X20_POWER_PARASITE ) 
			usart_write( "parasitaerer" );
		else usart_write( "externer" ); 
		usart_write( " Spannungsversorgung. " );

		// T messen
		if ( DS18X20_start_meas( DS18X20_POWER_PARASITE, &gSensorIDs[i][0] ) == DS18X20_OK ) {
				_delay_ms(DS18B20_TCONV_12BIT);
				if ( DS18X20_read_meas( &gSensorIDs[i][0], &subzero,
						&cel, &cel_frac_bits) == DS18X20_OK ) {
					DS18X20_show_id_uart( &gSensorIDs[i][0], OW_ROMCODE_SIZE );
					TWert = DS18X20_temp_to_decicel(subzero, cel, cel_frac_bits);
					usart_write(" %i %i.%4i C %i",subzero, cel, cel_frac_bits,TWert);
				}
				else usart_write(" CRC Error (lost connection?)");
			}
			else usart_write(" *** Messung fehlgeschlagen. (Kurzschluss?) ***");
	}
	
	#endif
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup tcpcmd
 * \b PING-Befehl Sende "Ping" an angegebene Adresse
 */
int16_t command_ping (char *outbuffer)
{
	uint32_t var = 0;

	if (getLong(&var))
	{
		unsigned long dest_ip=0;

		for (uint8_t i=0; i<4; ++i) {
			dest_ip += (var<<(8*i));
			if (!getLong(&var))
				break;
		}

		//ARP Request senden
		arp_request (dest_ip);
		
		//ICMP-Nachricht Type=8 Code=0: Echo-Anfrage
		//TODO: Sequenznummer, Identifier 
		icmp_send(dest_ip,0x08,0x00,1,1);
	}
	return 0;
}

//------------------------------------------------------------------------------
/**
 * \ingroup usartcmdline
 * \b HELP-Befehl Hilfstext ausgeben
 */
int16_t command_help (char *outbuffer)
{
	if (outbuffer)					// nur bei USART
		return cmd_502(outbuffer);

#if HELPTEXT
	unsigned char data;
	PGM_P helptest_pointer = helptext;
	
	do
	{
		data = pgm_read_byte(helptest_pointer++);
		usart_write("%c",data);
	}while(data != 0);
	return 0;
#endif
}

//------------------------------------------------------------------------------
/**
 * \ingroup usartcmdline
 * \b DISK-Befehl Informationen zur SD-Karte ausgeben
 */
int16_t print_disk_info(char *outbuffer)	//const struct fat16_fs_struct* fs)
{
#if !USE_MMC
	return cmd_502(outbuffer);
#else
	if (!cwdir_ptr)
        return 0;

	if (outbuffer)					// nur bei USART
		return cmd_502(outbuffer);

    struct sd_raw_info disk_info;
    if(!sd_raw_get_info(&disk_info)) {
	    usart_write("\r\nDisk kann nicht gelesen werden.");
        return 0;
	}

    usart_write("\r\nmanuf: 0x%x",disk_info.manufacturer);
    usart_write("\r\noem:     %s",disk_info.oem);
    usart_write("\r\nprod:    %s",disk_info.product);
    usart_write("\r\nrev:     %i",disk_info.revision);
    usart_write("\r\nserial:  %l",disk_info.serial);
    usart_write("\r\ndate:    %i/%i",disk_info.manufacturing_month,disk_info.manufacturing_year);
    usart_write("\r\nsize:    %l",disk_info.capacity);
    usart_write("\r\ncopy:    %i",disk_info.flag_copy);
    usart_write("\r\nwr.pr.:  %i/%i",disk_info.flag_write_protect_temp,disk_info.flag_write_protect);
    usart_write("\r\nformat:  %i",disk_info.format);
//    uart_puts_p(PSTR("free:   ")); uart_putdw_dec(fat16_get_fs_free(fs)); uart_putc('/');
//                                   uart_putdw_dec(fat16_get_fs_size(fs)); uart_putc('\n');
	return 0;
#endif
}
