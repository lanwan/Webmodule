/*----------------------------------------------------------------------------
 Copyright:      W.Wallucks
 known Problems: ? // siehe TODO
 Version:        23.05.2008
 Description:    TCP Service für AVR-Miniwebserver
 				 Telnet CMD-Interpreter und FTP-Server

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
----------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "../config.h"

#if TCP_SERVICE || DOXYGEN

#include "../stack.h"
#include "../sdkarte/fat16.h"
#include "../sdkarte/sdcard.h"
#include "../usart.h"
#include "tcpsrv.h"
#include "tcpcmd.h"

/**
 * \addtogroup tcpsrv	TCP-Service für Telnet CMD-Line und FTP
 *
 * @{
  */

/**
 * \file
 * tcpsrv.c
 *
 * \author W.Wallucks
 */

/**
 * \addtogroup tcpservice
 *	Hilfs-Funktionen für TCP-Service
 */

/**
 * @}
 */

/********************
* 	Vars
*/
TCPSRV_STATUS tcpsrv_status;

extern volatile unsigned int variable[MAX_VAR];


/********************
* 	lokale Prototypen
*/
static void datachannel(unsigned char);
static void _respond(uint8_t, uint16_t, unsigned char);
static void respond_226(unsigned char);
static void respond_P(uint8_t, const char *, unsigned char);
static void respond(uint8_t, const char *, unsigned char);

#define CTRL_CHANNEL	0
#define DATA_CHANNEL	1

/**************************************************/

/**
 *	\ingroup tcpsrv
 *	Initialisierung des TCP-Servers
 */
void tcpsrvd_init(void)
{
	//Serverport und Anwendung in TCP Anwendungsliste eintragen
	add_tcp_app (21, (void(*)(unsigned char))tcpsrvd);
	add_tcp_app (2100, (void(*)(unsigned char))datachannel);
	add_tcp_app (MYTCP_PORT, (void(*)(unsigned char))tcpsrvd);

	#if FTP_ANONYMOUS
	tcpsrv_status.userOK = 0;
	tcpsrv_status.loginOK = 0;
	#endif

	tcpsrv_status.tcpindex = 0xff;
	tcpsrv_status.datafile = 0;
}

/**
 *	\ingroup tcpsrv
 *	Telnet/FTP Befehl ausführen
 */
uint16_t doSrvCmd(uint8_t index, unsigned char *cmdstr)
{
	int16_t len = 0;
	uint8_t i = 0;

	while (pgm_read_word(&CMD_TABLE[i].cmd) && strcasecmp_P((char *)cmdstr,(char *)pgm_read_word(&CMD_TABLE[i].cmd))) i++;
	// Wenn Befehlsindex zu gross, dann mit Fehler beenden
	if (!pgm_read_word(&CMD_TABLE[i].cmd)) {
		strcpy_P((char *)&eth_buffer[TCP_DATA_START],PSTR("502 not implemented.\r\n"));
		return strlen((char *)&eth_buffer[TCP_DATA_START]);
	}

	argv = (char *)tcpsrv_status.argvbuffer;	// Command-Parameter setzen
	len = ((cmd_fp)pgm_read_word(&CMD_TABLE[i].fp))((char *)&eth_buffer[TCP_DATA_START_VAR]); // Befehl ausführen und Ergebnis zurückgeben

	if (len < 0) {	// negativer Wert beendet Verbindung
		//tcp_entry[index].app_status = 0xFFFD;	// 123456789.123456789.123456789.
		memcpy_P(&eth_buffer[TCP_DATA_START],PSTR("bye, bye ...\r\n"),14);
		tcp_Port_close(index);
		len = 14;
	}
	return (len);
}

/**
 *	\ingroup tcpsrv
 *	tcp Server (CTRL-Channel)
 */
void tcpsrvd(uint8_t index)
{
	TCPSRV_DEBUG("\r\n---- TCP SrvCTRL %i from: %i  Flags: 0x%x seq: %u ack:%u",
										index,LBBL_ENDIAN_INT(tcp_entry[index].src_port),tcp_entry[index].status,
										LBBL_ENDIAN_LONG(tcp_entry[index].seq_counter),
										LBBL_ENDIAN_LONG(tcp_entry[index].ack_counter) );

	if (tcp_entry[index].status & FIN_FLAG) {	// Wird Verbindungsabbau signalisiert?
		TCPSRV_DEBUG("\r\nCTRL-channel %i closed.",index);
		tcpsrv_status.tcpindex = 0xff;
		#if FTP_ANONYMOUS
		tcpsrv_status.userOK = 0;
		tcpsrv_status.loginOK = 0;
		#endif

		return;
	}

	if (tcp_entry[index].app_status <= 1) {	// erstmaliger Aufruf der Anwendung -> Begrüßung senden
		tcp_entry[index].app_status = 1;
		respond_P(CTRL_CHANNEL, PSTR("220 Server bereit\r\n"),index);
		tcpsrv_status.transfermode = 0;	// ASCII mode
		tcpsrv_status.tcpindex = index;	// letzten socket merken
		return;
	}
	// else if(tcp_entry[index].app_status > 1) ...

	if (tcp_entry[index].status&PSH_FLAG) {		// Ist das Paket für die Anwendung bestimmt ?
		tcp_entry[index].app_status = 2;		// nicht automatisch weiterzählen

		/*
		* eingehende Zeichen (bei TELNET 1 char pro Paket !) sammeln
		* und Cmd-Zeile interpretieren.
		* Cmd und Parameter getrennt erfassen.
		*/
		for (int a = TCP_DATA_START_VAR;a<(TCP_DATA_END_VAR);a++) {	// Schleife über alle Zeichen im Empfangspuffer

			if (eth_buffer[a] == '\r') {			// Zeilenende erkannt ?

				TCPSRV_DEBUG("\r\nTCP Server-CTRL cmd: %s argv: %s",tcpsrv_status.cmdbuffer,tcpsrv_status.argvbuffer);

				tcpsrv_status.tcpindex = index; 	// letzten socket merken

				// ausführen und Ergebnis im TCP-Paket zurückgeben
				_respond(CTRL_CHANNEL, doSrvCmd(index,(unsigned char *)tcpsrv_status.cmdbuffer),index);
				return;
			}

			else if ( tcpsrv_status.read_argv == 0 && eth_buffer[a] == ' ') {	// cmd Ende, Parameter folgt
				tcpsrv_status.read_argv = 1;
			}

			else if (eth_buffer[a] >= ' ') {		// CTRL-Zeichen ('\n', '\t' etc.) verwerfen

				// zum debuggen:
				//while(!(USR & (1<<UDRE)));		// Warten solange bis Zeichen gesendet wurde
				//UDR = eth_buffer[a];				// Ausgabe des Zeichens

				if (tcpsrv_status.read_argv) {
					tcpsrv_status.argvbuffer[tcpsrv_status.argvindex++] = eth_buffer[a];
					tcpsrv_status.argvbuffer[tcpsrv_status.argvindex] = 0;	// und gleich wieder terminieren
					if (tcpsrv_status.argvindex > MAXARGVBUFFER) {
						tcpsrv_status.argvindex= 0;			// Pufferüberlauf
						tcpsrv_status.argvbuffer[0] = 0;
						tcpsrv_status.read_argv = 0;
						break;
					}
				}

				else {
					tcpsrv_status.cmdbuffer[tcpsrv_status.cmdindex++] = eth_buffer[a];
					tcpsrv_status.cmdbuffer[tcpsrv_status.cmdindex] = 0;	// und gleich wieder terminieren
					if (tcpsrv_status.cmdindex > MAXCMDBUFFER) {
						// TODO: Fehler melden

						tcpsrv_status.cmdindex= 0;			// cmd Puffer resetten
						tcpsrv_status.read_argv = 0;
						tcpsrv_status.cmdbuffer[0] = 0;
					}
				}
			}
		}

		TCPSRV_DEBUG("\r\nTCP Server-CTRL sende ACK");
		tcp_entry[index].status =  ACK_FLAG;	// empfangenes Paket bestätigen
		create_new_tcp_packet(0,index);			// löscht .status flags
		tcpsrv_status.ctl_waitack = 0;
		tcp_entry[index].time = TCP_TIME_OFF;	// Timer ausschalten
		//tcp_entry[index].time = MAX_TCP_PORT_OPEN_TIME;
		return;
	}

	// ist dies ein ACK von einem gesendeten Paket
	if ((tcp_entry[index].status&ACK_FLAG)) {	//&& (tcpsrv_status.ctl_waitack)
		tcpsrv_status.ctl_waitack = 0;
		tcp_entry[index].time = TCP_TIME_OFF;	// Timer ausschalten
		TCPSRV_DEBUG("\r\nACK: dest: %i src %i seq %l ack %l",tcp_entry[index].dest_port, tcp_entry[index].src_port,
													   LBBL_ENDIAN_LONG(tcp_entry[index].seq_counter),
													   tcp_entry[index].ack_counter);

		if (tcpsrv_status.send_226) {		// muss noch "226 Transfer Ende" gesendet werden?
			TCPSRV_DEBUG("\r\nTCP Server-CTRL sende 226");
			tcpsrv_status.send_226 = 0;
			respond_226(index);
		}
		return;
	}
	
	// Timeout kein ack angekommen (kann nicht auftreten, da Timer ausgeschaltet!)
	/*
	if (tcp_entry[index].status == 0) {
		TCPSRV_DEBUG("\r\nTCP Server-CTRL TIMEOUT");
		//Daten nochmal senden ?
		if (tcpsrv_status.ctl_waitack) {
			// letzten Befehl nochmals ausführen und Ergebnis senden
			_respond(CTRL_CHANNEL, doSrvCmd(index,(unsigned char *)tcpsrv_status.cmdbuffer),index);
			tcp_entry[index].time = MAX_TCP_PORT_OPEN_TIME;
		}
		else {
			// Port schließen
			tcp_Port_close(index);
		}
	}
	*/

	return;
}

/*
*	Antwortstring(s) senden
*/
void respond_P(uint8_t channel, const char *pstr, unsigned char index)
{
	uint16_t len = strlen_P(pstr);
	memcpy_P(&eth_buffer[TCP_DATA_START],pstr,len);

	_respond(channel, len,index);
}

void respond(uint8_t channel, const char *pstr, unsigned char index)
{
	uint16_t len = strlen(pstr);
	memcpy(&eth_buffer[TCP_DATA_START],pstr,len);
	_respond(channel, len,index);
}

void _respond(uint8_t channel, uint16_t len, unsigned char index)
{
	tcp_entry[index].status =  ACK_FLAG | PSH_FLAG;
	create_new_tcp_packet(len,index);

	eth_buffer[TCP_DATA_START+len] = 0;	// nur für debug
	TCPSRV_DEBUG("\r\nTCPSRV_DEBUG respond %i (%i): %s",index, len, &eth_buffer[TCP_DATA_START]);

	if (channel == CTRL_CHANNEL) {
		tcpsrv_status.ctl_waitack = 1;		// dieses Paket sollte bestätigt werden
		tcp_entry[index].time = TCP_TIME_OFF;	// Timer ausschalten
		tcpsrv_status.cmdindex= 0;			// auf erstes Zeichen setzen
		tcpsrv_status.cmdbuffer[0] = 0;		// und cmd-string terminieren
		tcpsrv_status.argvindex= 0;			// cmd Puffer resetten
		tcpsrv_status.argvbuffer[0] = 0;
		tcpsrv_status.read_argv = 0;
	}
	else
		tcpsrv_status.data_waitack = 1;		// dieses Paket sollte bestätigt werden
}

/*
*	Datei in Ethernet Sendepuffer einlesen
*/
uint16_t read_file_to_eth_buffer(void)
{
	int16_t i=0;

	if (!tcpsrv_status.datafile)
		tcpsrv_status.datafile = f16_open((char *)tcpsrv_status.fname,"r");

	/*
	** alte Pointer auf Datenanfang in Datei nachziehen
	*/
	tcpsrv_status.old_charcount = tcpsrv_status.charcount;

	/*
	*  positionieren und lesen
	*/
	if (f16_fseek(tcpsrv_status.datafile, tcpsrv_status.old_charcount, FAT16_SEEK_SET)) {
		i = fat16_read_file(tcpsrv_status.datafile, (unsigned char *)&eth_buffer[TCP_DATA_START], MTU_SIZE-(TCP_DATA_START+10));
		if (i<0)
			i = 0;
	}
	tcpsrv_status.charcount += i;
	//f16_close(tcpsrv_status.datafile);
	return i; // Zahl der kopierten Zeichen
}

/*
* 	mit 226 antworten
*	setzt Flag falls ACK noch aussteht
*	oder sendet "226 Transfer complete"
*/
void respond_226(unsigned char index)
{
	TCPSRV_DEBUG("\r\nTCP Server %i respond 226 wait: %i",index, tcpsrv_status.ctl_waitack);
	if (index < MAX_TCP_ENTRY) {
		if (tcpsrv_status.ctl_waitack)
			tcpsrv_status.send_226 = 1;			// nach nächstem ACK wird "226 Transfer Ende" gesendet
		else
			respond_P(CTRL_CHANNEL, PSTR("226 Transfer complete. Closing data connection\r\n"),index);
	}
}


/**
 *	\ingroup tcpsrv
 * 	Datenkanal auf Port 2100
 */
void datachannel(unsigned char index)
{
	TCPSRV_DEBUG("\r\n---- Datachannel %i State: %i Flags: 0x%x seq: %u ack:%u",
										index,tcpsrv_status.data_state,tcp_entry[index].status,
										LBBL_ENDIAN_LONG(tcp_entry[index].seq_counter),
										LBBL_ENDIAN_LONG(tcp_entry[index].ack_counter) );

	switch(tcpsrv_status.data_state)
	{
		case 1:		// LIST
			if (tcp_entry[index].app_status < 1) {
				// erster Aufruf - initialisieren
				tcp_entry[index].app_status = 1;
				tcpsrv_status.data_waitack = 0;
				tcp_entry[index].time = TCP_MAX_ENTRY_TIME * 2;

			}

			/*
			if(tcpsrv_status.data_waitack != 0) {				// warten wir auf ACK vom letzten Paket ?
				if (tcp_entry[index].status&ACK_FLAG) {
					// letztes Paket wurde bestätigt.
					// Zeiger nachziehen
					tcpsrv_status.old_entry_cluster = tcpsrv_status.entry_cluster;
					tcpsrv_status.old_entry_offset = tcpsrv_status.entry_offset;
				}

				if(tcp_entry[index].status==0) {
					// keine Bestätigung für Paket. (TIMEOUT ?)
					// Zeiger auf letztes Paket zurücksetzen
					tcpsrv_status.entry_cluster = tcpsrv_status.old_entry_cluster;
					tcpsrv_status.entry_offset = tcpsrv_status.old_entry_offset;
				}
				tcpsrv_status.data_waitack = 0;
			}
			*/

			int16_t bufferlen = cmd_MMCdir((char *)&eth_buffer[TCP_DATA_START]);

			if ( !tcpsrv_status.LISTcontinue ) {				// Ende der Dateiliste?
				tcpsrv_status.data_state = 0;					// nächster Status ist "idle"
				tcp_entry[index].status = ACK_FLAG | FIN_FLAG;	// Ende senden
				tcp_entry[index].app_status = 0xFFFF;			// Anwendung beendet.
			}
			else {
				tcp_entry[index].status = ACK_FLAG;
				tcpsrv_status.data_waitack = 1;					// flag setzen, dass wir auf ACK warten
			}

			//TCPSRV_DEBUG("\r\nFTP_LIST:(%i) #entry:%i\r\n%s",bufferlen,tcpsrv_status.direntry_num,&eth_buffer[TCP_DATA_START]);
			create_new_tcp_packet(bufferlen,index);				// Senden

			if ( !tcpsrv_status.LISTcontinue ) {				// Ende der Dateiliste?
				respond_226(tcpsrv_status.tcpindex);			// Ende der Übertragung auf control socket melden
				break;
			}
			break;

		case 2:		// RETR

			if (tcp_entry[index].app_status < 1) {

				// erster Aufruf - initialisieren
				tcp_entry[index].app_status = 1;
				tcpsrv_status.old_charcount = 0;
				tcpsrv_status.charcount = 0;
				tcpsrv_status.data_waitack = 0;
			}
			
			if(tcpsrv_status.data_waitack != 0) {				// warten wir auf ACK vom letzten Paket ?
				if (tcp_entry[index].status&ACK_FLAG) {
					// letztes Paket bestätigt.
					// Zeiger nachziehen
					tcpsrv_status.old_charcount = tcpsrv_status.charcount;
				}

				if(tcp_entry[index].status==0) {
					// keine Bestätigung für Paket. (TIMEOUT ?)
					// Zeiger auf letztes Paket zurücksetzen
					tcpsrv_status.charcount = tcpsrv_status.old_charcount;
				}
				tcpsrv_status.data_waitack = 0;
			}

			// Datendatei in eth-Puffer einlesen
			unsigned int count = read_file_to_eth_buffer();

			if(count < MTU_SIZE-(TCP_DATA_START+10)) {			// ist das das letzte Paket
				// Ende der Datenübertragung
				tcpsrv_status.data_state = 0;					// nächster Status ist "idle"
				tcp_entry[index].status = ACK_FLAG | FIN_FLAG;	// Ende senden
				tcp_entry[index].app_status = 0xFFFF;			// Anwendung beendet.
				if (tcpsrv_status.datafile) {					// Filepointer freigeben
					f16_close(tcpsrv_status.datafile);
					tcpsrv_status.datafile = 0;
				}
			}
			else {
				tcp_entry[index].status = ACK_FLAG;
				tcpsrv_status.data_waitack = 1;					// flag setzen, dass wir auf ACK warten
			}

			create_new_tcp_packet(count,index);					// Senden
			if(count < MTU_SIZE-(TCP_DATA_START+10)) {			// ist das das letzte Paket
				respond_226(tcpsrv_status.tcpindex);			// Ende der übertragung auf control socket melden
			}
			break;

		case 3:		// STOR
			if (!tcpsrv_status.datafile)
				tcpsrv_status.datafile = f16_open((char *)tcpsrv_status.fname,"a");

			TCPSRV_DEBUG("\r\nTCPSRV_DEBUG STOR %s %i Bytes",tcpsrv_status.fname,(TCP_DATA_END_VAR)-(TCP_DATA_START_VAR));
			if (tcpsrv_status.datafile) {
				TCPSRV_DEBUG("\r\nTCPSRV_DEBUG STOR %i bis %i",TCP_DATA_START_VAR,TCP_DATA_END_VAR);
				fat16_write_file(tcpsrv_status.datafile, (unsigned char *)&eth_buffer[TCP_DATA_START_VAR],(TCP_DATA_END_VAR)-(TCP_DATA_START_VAR));
				//sd_raw_sync();	// flush databuffers
			}

			uint8_t fin_test = tcp_entry[index].status & FIN_FLAG;

			tcp_entry[index].status =  ACK_FLAG;
			create_new_tcp_packet(0,index);
			TCPSRV_DEBUG("\r\nTCPSRV_DEBUG STOR send ACK");

			/*
			** je nach Dateigröße sendet FileZilla das FIN_FLAG mit dem letzten Datenpaket
			** oder auch getrennt als eigenständiges Paket
			*/
			if (fin_test) {									// Wird Verbindungsabbau signalisiert?
				TCPSRV_DEBUG("\r\ndata-channel %i closed Port: %i",index,tcp_entry[index].src_port);
				respond_226(tcpsrv_status.tcpindex);		// im Ctrl-Kanal noch mitteilen
				tcpsrv_status.data_state = 0;				// idle setzen
				if (tcpsrv_status.datafile) {
					f16_close(tcpsrv_status.datafile);
					tcpsrv_status.datafile = 0;
				}
				return;
			}
		break;

		case 0:
		default:
			if (!(tcp_entry[index].status & FIN_FLAG)) {	// Wird Verbindungsabbau signalisiert?
				// ACK wird automatisch in stack.c gesendet
				return;
			}

			// ACK vom FIN_FLAG ? -> Port wird vom client noch geschlossen; oder Timeout
			if ((tcp_entry[index].status&ACK_FLAG)) {
				tcp_entry[index].time = MAX_TCP_PORT_OPEN_TIME;
			}

			TCPSRV_DEBUG("\r\nDatenkanal %i closed. App: %x",index,tcp_entry[index].app_status);
			tcp_entry[index].app_status = 0xFFFF;			// Anwendung beendet.
			tcpsrv_status.data_waitack = 0;
			break;
	}
}

#endif
