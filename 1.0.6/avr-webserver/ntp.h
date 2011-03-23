/*----------------------------------------------------------------------------
 Copyright:      Radig Ulrich  mailto: mail@ulrichradig.de
 Author:         Radig Ulrich
 Remarks:        
 known Problems: none
 Version:        12.11.2007
 Description:    NTP Client

 Dieses Programm ist freie Software. Sie k�nnen es unter den Bedingungen der 
 GNU General Public License, wie von der Free Software Foundation ver�ffentlicht, 
 weitergeben und/oder modifizieren, entweder gem�� Version 2 der Lizenz oder 
 (nach Ihrer Option) jeder sp�teren Version. 

 Die Ver�ffentlichung dieses Programms erfolgt in der Hoffnung, 
 da� es Ihnen von Nutzen sein wird, aber OHNE IRGENDEINE GARANTIE, 
 sogar ohne die implizite Garantie der MARKTREIFE oder der VERWENDBARKEIT 
 F�R EINEN BESTIMMTEN ZWECK. Details finden Sie in der GNU General Public License. 

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
 * ntp.h
 *
 * \author Ulrich Radig
 */


#if USE_NTP
#ifndef _NTPCLIENT_H
	#define _NTPCLIENT_H

	//#define NTP_DEBUG usart_write
	#define NTP_DEBUG(...)

	#define NTP_CLIENT_PORT		2300
	#define NTP_SERVER_PORT		123

	//Refresh alle 7200 Sekunden also alle 2 Stunden
	#define NTP_REFRESH 7200


	#define NTP_IP_EEPROM_STORE 	50

	unsigned char ntp_server_ip[4];
	volatile unsigned int ntp_timer;
	
	void ntp_init(void);
	void ntp_request(void); 
	void ntp_get(unsigned char);
	
	#define GMT_TIME_CORRECTION 3600	//!< Zeitkorrektur zu UTC (1 Stunde in Sekunden)
	
	struct NTP_GET_Header {
		char dummy[40];
		unsigned long rx_timestamp;
	};
	
#endif
#endif //USE_NTP

/**
 * @}
 */
