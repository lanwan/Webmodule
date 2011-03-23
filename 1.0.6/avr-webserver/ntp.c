/*----------------------------------------------------------------------------
 Copyright:      Radig Ulrich  mailto: mail@ulrichradig.de
 Author:         Radig Ulrich
 Remarks:        
 known Problems: none
 Version:        12.11.2007
 Description:    NTP Client

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
------------------------------------------------------------------------------*/

/**
 * \addtogroup funcs Hilfsfunktionen
 *
 * @{
 */

/**
 * \addtogroup time
 *
 */

 /**
 * \file
 * ntp.c
 *
 * \author Ulrich Radig
 */


/**
 * @}
 */

#include "config.h"

#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "stack.h"
#include "timer.h"
#include "dns.h"
#include "ntp.h"

#if USE_NTP || DOXYGEN
volatile unsigned int ntp_timer;	//!> Zeitspanne in Sekunden nachdem die Zeit über NTP neu gestellt wird

//----------------------------------------------------------------------------
//
PROGMEM char  NTP_Request[] = {	0xd9,0x00,0x0a,0xfa,0x00,0x00,0x00,0x00,
									0x00,0x01,0x04,0x00,0x00,0x00,0x00,0x00,
									0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
									0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
									0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
									0xc7,0xd6,0xac,0x72,0x08,0x00,0x00,0x00,
									'%','E','N','D' };

unsigned char ntp_server_ip[4];	//!< die IP eines NTP-Servers

//----------------------------------------------------------------------------
/**
 *	\ingroup time
 *	Initialisierung des NTP Ports (für Daten empfang)
 *
 */
void ntp_init (void)
{
	//Port in Anwendungstabelle eintragen für eingehende NTP Daten!
	add_udp_app (NTP_CLIENT_PORT, (void(*)(unsigned char))ntp_get);
	
	// DNS Auflösung erzwingen
	(*((unsigned long*)&ntp_server_ip[0])) = 0L;
	ntp_request();

	return;
}

//----------------------------------------------------------------------------
/**
 *	\ingroup time
 *	Anforderung der aktuellen Zeitinformationen von einem NTP Server
 *
 */
void ntp_request (void)
{
	//oeffnet eine Verbindung zu einem NTP Server
	unsigned int byte_count;
	uint32_t tmp_ip = (*(uint32_t*)&ntp_server_ip[0]);
		
	if ( tmp_ip == 0L ) {
		dns_request(NTP_SERVER, (uint32_t *)&ntp_server_ip[0]);
		ntp_timer = 4; // neuer Versuch nach 4 Sekunden
		NTP_DEBUG(" no IP **\r\n");
		return;
	}
	else {
		uint32_t ee_ip = get_eeprom_value(NTP_IP_EEPROM_STORE,NTP_IP);
		NTP_DEBUG("IP: %1i.%1i.%1i.%1i",ntp_server_ip[0],ntp_server_ip[1],ntp_server_ip[2],ntp_server_ip[3]);
		if (tmp_ip != ee_ip) {
			//value ins EEPROM schreiben
			for (uint16_t count = 0; count<4; count++)
			{
				eeprom_busy_wait ();
				eeprom_write_byte((unsigned char *)(count+NTP_IP_EEPROM_STORE),ntp_server_ip[count]);
			}
			NTP_DEBUG(" saved");
		}
	}

	//ARP Request senden
	if (arp_request(tmp_ip) == 1)
	{	
		//Interrupt Deaktivieren da Buffer gerade zum senden benutzt wird!
		//ETH_INT_DISABLE;
		PGM_P ntp_data_pointer = NTP_Request;
		for (byte_count = 0;byte_count<(MTU_SIZE-(UDP_DATA_START));byte_count++)
		{
			unsigned char b;
			b = pgm_read_byte(ntp_data_pointer++);
			eth_buffer[UDP_DATA_START + byte_count] = b;
			//wurde das Ende des Packetes erreicht?
			//Schleife wird abgebrochen keine Daten mehr!!
			if (strncasecmp_P("%END",ntp_data_pointer,4)==0)
			{	
				byte_count++;
				break;
			}
		}
		
		create_new_udp_packet(byte_count,NTP_CLIENT_PORT,NTP_SERVER_PORT,tmp_ip);
		//ETH_INT_ENABLE;
		NTP_DEBUG("** NTP Request gesendet! **\r\n");
		(*(unsigned long*)&ntp_server_ip[0]) = 0L;	// neuen DNS request bei nächster NTP Abfrage erzwingen
		return;
	}
	NTP_DEBUG("Kein NTP Server gefunden!!\r\n");
	return;
}

//----------------------------------------------------------------------------
/**
 *	\ingroup time
 *	Empfang der Zeitinformationen von einem NTP Server<br>
 *	korrigiert die empfangene Zeit auf Sommerzeit
 *
 *	\param[in]	index Zeiger auf den verwendeten Eintrag in der UDP-Tabelle
 *
 */
void ntp_get (unsigned char index)
{
	NTP_DEBUG("** NTP DATA GET! **\r\n");
		
	struct NTP_GET_Header *ntp;
	ntp = (struct NTP_GET_Header *)&eth_buffer[UDP_DATA_START];

	ntp->rx_timestamp = HTONL(ntp->rx_timestamp);
	ntp->rx_timestamp += GMT_TIME_CORRECTION; //  UTC +1h
	time = ntp->rx_timestamp;
	
	uint16_t tage = (unsigned long)(time/86400);	// Tage seit 1.1.1900
	tage -= 39445;	// 108 Jahre abziehen -> Tag 0 ist 31.12.2007

	unsigned char yy = 8;		// 2008 ist Offset
	while (tage > 365) {
		tage -= 365;
		if ( yy % 4 == 0) {
			tage--;		// Schaltjahr
		}
		++yy;
	}
	TM_YY = yy;

	NTP_DEBUG("\r\nJahr:%i Rest: %i\r\n",yy,tage);
	TM_SetDayofYear(tage);		// die restlichen Tage werden direkt gesetzt.
 
	TM_hh = (time/3600)%24;
	TM_mm = (time/60)%60;
	TM_ss = time %60;
	NTP_DEBUG("\n\rNTP TIME: %2i:%2i:%2i\r\n",TM_hh,TM_mm,TM_ss);

	// auf Sommerzeit korrigieren
	if(TM_MM > 2 && TM_MM < 11) {	// 11, 12, 1 und 2 haben keine Sommerzeit

		uint8_t sommerzeit = 1;		// restliche Monate haben prinzipiell Sommerzeit

		if( (TM_DD - TM_DOW >= 25) && (TM_DOW || TM_hh >= 2) ) {
			// nach 02h00 (UTC+01) des letzten Sonntags im Monat
			if( TM_MM == 10) {	// Oktober
				sommerzeit = 0;	// bereits Winter
			}
		}
		else {
			// vor 02h00 (UTC+01) des letzten Sonntags im Monat
			if( TM_MM == 3) {	// März
				sommerzeit = 0;	// noch Winter
			}
		}

		TM_hh += sommerzeit;	// Offset addieren

		// falls während der Sommerzeit zwischen 23h00 und 24h00 die Zeit neu gestellt wurde ...
		if (TM_hh > 23) {
			TM_hh -= 24;
			TM_AddOneDay();
		}
	}

	machineStatus.LogInit = true;		// neue Logdatei initialisieren - wird in Mainloop erledigt

}

#endif //USE_NTP
