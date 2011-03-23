/*----------------------------------------------------------------------------
 Copyright:      W.Wallucks
 Remarks:        
 known Problems: none
 Version:        04.05.2008
 Description:    messen und regeln mit dem Webmodul

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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "config.h"
#include "messung.h"
#include <util/delay.h>

#include "networkcard/enc28j60.h"
#include "stack.h"
#include "timer.h"
#include "httpd.h"

#if USE_MMC
#include "sdkarte/fat16.h"
#include "sdkarte/sdcard.h"
#endif

#include "usart.h"

//#define MES_DEBUG	usart_write 
#define MES_DEBUG(...)	

/**
 * \addtogroup messen Datenaufnahme und Logging
 *
 * @{
  */

/**
 * \file
 * Messung.c
 *
 * \author W.Wallucks
 */

/**
 * @}
 */


#if USE_OW || DOXYGEN
#include "1-wire/ds18x20.h"

// Variable
volatile int16_t 	ow_array[MAXSENSORS];	//!< Speicherplatz für 1-wire Sensorwerte

PROGMEM	uint8_t		DS18B20IDs[MAXSENSORS+1][OW_ROMCODE_SIZE] = {
							OW_ID_T01,		// 1. DS18B20
							OW_ID_T02,		// 2. DS18B20 ...
							OW_ID_Last };	// Endmarker
#endif

#define SENS_INT_ENABLE() 	PCICR |= (1<<SENS_INTENA)
#define SENS_INT_DISABLE() 	PCICR &= ~(1<<SENS_INTENA)

/**
 *	\ingroup messen
 *	Initialisierung der Ein- und Ausgänge
 */
void messung_init(void)
{
	SENS_DDR &= ~(SENS_ACTIVE_PINS);		// aktive Sensoren auf Eingang
	SENS_PULLUP |= (SENS_ACTIVE_PINS); 		// Pullups einschalten
	SENS_INTMASK |= SENS_ACTIVE_PINS;		// auf aktive Eingänge reagieren
	SENS_INT_ENABLE();

	// Ausgänge für Relais schalten
	OUT_DDR |= (1<<PORT_SCHALTER1) | (1<<PORT_SCHALTER2) | (1<<PORT_SCHALTER3);
	S1Aus(); S2Aus(); S3Aus(); 				// alles ausschalten

	machineStatus.Tlesen 		= 1;		// alle Messwerte neu lesen
	machineStatus.LogSchreiben 	= 1;
	machineStatus.PINAStatus	= PINA;		// aktuellen Port-Status merken
	machineStatus.PINCStatus	= PINC;
	machineStatus.PINDStatus	= PIND;
	machineStatus.sendmail		= 0;		// keine E-Mail zu senden (oder REBOOT-Mail ?)

	#if USE_OW
	set_SensorResolution();
	#endif

	log_init();
	#if USE_SCHEDULER
	initSchaltzeiten(0);
	#endif
}

#if USE_OW || DOXYGEN

/**
 *	\ingroup messen
 *	neue Temperaturmessung starten
 *	\returns benötigte Wartezeit
 */
uint8_t start_OW(void)
{
#if OW_EXTERN_POWERED
	// T messen: alle gleichzeitig starten
	if ( DS18X20_start_meas( DS18X20_POWER_EXTERN, 0 ) == DS18X20_OK ) {
		// warten bis Messung fertig ist.
		// DS18S20 braucht die vollen 750ms
		// - daher vorsichtshalber volle 750ms warten, falls unterschiedlich bestückt
		return (DS18B20_12_BIT/TIMERBASE);	// Wartezeit zurückgeben
	}

	MES_DEBUG("\r\n*** Messung fehlgeschlagen. (Kurzschluss?) ***");
	return 0;	// es wird kein Timer eingeschaltet
#else
	return 1;	// Timer auf Minimalzeit einschalten
#endif
}

/**
 *	\ingroup messen
 *	bekannte DS18x20 Temperatursensoren auslesen
 */
void lese_Temperatur(void)
{
	uint8_t i = 0;
	uint16_t TWert;
	uint8_t subzero, cel, cel_frac_bits;
	uint8_t tempID[OW_ROMCODE_SIZE];

	memcpy_P(tempID,DS18B20IDs[0],OW_ROMCODE_SIZE);	// erste ID ins RAM holen

#if OW_EXTERN_POWERED
	while (tempID[0] != 0) {
		if ( DS18X20_read_meas( tempID, &subzero,&cel, &cel_frac_bits) == DS18X20_OK ) {
			TWert = DS18X20_temp_to_decicel(subzero, cel, cel_frac_bits);
			if (subzero)
				TWert *= (-1);
			ow_array[i] = TWert;
		}
		else {
			MES_DEBUG("\r\nCRC Error (lost connection?) ");
			DS18X20_show_id_uart( tempID, OW_ROMCODE_SIZE );
		}

		memcpy_P(tempID,DS18B20IDs[++i],OW_ROMCODE_SIZE);	// nächste ID ins RAM holen
	}

#else
	while (tempID[0] != 0) {
		// T messen (einzeln starten und messen bei parasitärer Spannungsversorgung)
		if ( DS18X20_start_meas( DS18X20_POWER_PARASITE, tempID ) == DS18X20_OK ) {

			// warten bis Messung fertig ist. DS18S20 braucht die vollen 750ms
			if (tempID[0] == DS18B20_ID)
				_delay_ms(DS18B20_TCONV_10BIT);
			else
				_delay_ms(DS18S20_TCONV);

			if ( DS18X20_read_meas( tempID, &subzero,&cel, &cel_frac_bits) == DS18X20_OK ) {
				TWert = DS18X20_temp_to_decicel(subzero, cel, cel_frac_bits);
				if (subzero)
					TWert *= (-1);
				ow_array[i] = TWert;
			}
			else {
				MES_DEBUG("\r\nCRC Error (lost connection?) ");
				DS18X20_show_id_uart( tempID, OW_ROMCODE_SIZE );
			}
		}
		else MES_DEBUG("\r\n*** Messung fehlgeschlagen. (Kurzschluss?) ***");

		memcpy_P(tempID,DS18B20IDs[++i],OW_ROMCODE_SIZE);	// nächste ID ins RAM holen
	}
#endif
}
#endif

#if USE_OW || DOXYGEN
/**
 *	\ingroup messen
 *
 *  DS18B20 auf 10bit Genauigkeit setzen (das reicht im Normalfall!)
 *  das verkürzt die Messzeit pro Sensor (750 msec bei 12 bit) auf ein Viertel!
 */
void set_SensorResolution(void)
{
	uint8_t i = 0;
	uint8_t tempID[OW_ROMCODE_SIZE];

	memcpy_P(tempID,DS18B20IDs[0],OW_ROMCODE_SIZE);	// erste ID ins RAM holen

	while (tempID[0] == DS18B20_ID) {
		// set 10-bit Resolution - Alarm-low-T 0 - Alarm-high-T 85
		DS18X20_write_scratchpad( tempID , 0, 85, DS18B20_10_BIT);

		memcpy_P(tempID,DS18B20IDs[++i],OW_ROMCODE_SIZE);	// nächste ID ins RAM holen
	}
}
#endif


/**
 *	\ingroup messen
 * ISR für Pinchange-Signal an Port A
 */
ISR(PCINT0_vect)
{
	_delay_ms(10);	// prellen abwarten

	machineStatus.PINAStatus	= PINA;		// aktuellen Port-Status merken
}

/**
 *	\ingroup messen
 * ISR für Pinchange-Signal an Port C
 *
 * In <tt>machineStatus.PINCchanged</tt> wird vermerkt, ob es eine
 * Änderung der Eingänge an PINC gab<br>
 * und in <tt>machineStatus.PINCStatus</tt> wird der aktuelle
 * Zustand der Eingänge gespeichert.
 */
ISR(PCINT2_vect)
{
	_delay_ms(5);	// prellen abwarten

	machineStatus.PINCchanged = SENS_Read ^ machineStatus.PINCStatus;
	machineStatus.PINCStatus = SENS_Read;
}

/**
 *	\ingroup messen
 * ISR für Pinchange-Signal an Port D
 */
ISR(PCINT3_vect)
{
	machineStatus.PINDStatus = PIND;
}


#if USE_SCHEDULER || DOXYGEN
/*
 *	\ingroup messen
 * Schaltzeiten von SD-Karte initialisieren
 */
int16_t initSchaltzeiten(char *outbuffer)
{
	if (cwdir_ptr) {	// falls SD-Karte vorhanden

		int ch;
		int8_t index1 = 0, index2 = 0;
		int16_t itmp = 0;

		uint8_t schaltzeit = 0;
    	uint8_t Wochentag = 0x7f;
		SOLL_STATUS Zustand;

		File *inifile = f16_open(SCHED_INIFILE,"r");
		if (!inifile) return 0;

		f16_fseek(inifile,0,FAT16_SEEK_SET);
		//usart_write_str("\r\nINI-File:\r\n");
		ch = f16_getc(inifile);
		while ( ch > 0 ) {
			//usart_write_char((char)ch);

			if (isdigit(ch)) {
				itmp *= 10;
				itmp += (ch - '0');
			}

			if (ispunct(ch)) {		// Feldtrenner ?
				switch (index2) {	// Wert setzen und Zeiger erhöhen
					case 0:
						schaltzeit = (uint8_t)itmp;
						break;
					case 1:
						Wochentag = (uint8_t)itmp;
						break;
					case 2:
						Zustand.Schalter1 = (uint8_t)itmp;
						break;
					case 3:
						Zustand.Schalter2 = (uint8_t)itmp;
						break;
					case 4:
						Zustand.Schalter3 = (uint8_t)itmp;
						break;
					case 6:
						Zustand.TReglerWert = (uint8_t)itmp;
						break;
				}
				++index2;
				itmp = 0;
			}

			if (ch == '\r') {
				// aktuelle Werte schreiben
				TM_SchaltzeitSet(index1, schaltzeit, Wochentag, &Zustand);

				ch = f16_getc(inifile);	// read '\n'
				if (++index1 >= TM_MAX_SCHALTZEITEN) {
					break;
				}
				itmp = 0;
				index2 = 0;	// nächsten Eintrag von vorne beginnen
			}
		ch = f16_getc(inifile);
		}

		f16_close(inifile);

		// aktuellen Sollzustand einstellen
		SOLL_STATUS aktSoll;
		TM_SollzustandGetAktuell(&aktSoll);
		regelAnlage(&aktSoll);
	}
	return 0;
}

/**
 *	\ingroup messen
 * Anlage regeln nach Sollwerten
 */
void regelAnlage(SOLL_STATUS *aktSoll)
{
	if (aktSoll->Schalter1) {	// Schalter1 schalten
		S1An();
	}
	else {
		S1Aus();
	}

	if (aktSoll->Schalter2) {	// Schalter2 schalten
		S2An();
	}
	else {
		S2Aus();
	}

	if (aktSoll->Schalter3) {	// Schalter3 schalten
		S3An();
	}
	else {
		S3Aus();
	}

	// Protokoll
	#if USE_LOGDATEI
	if (logStatus.logfile) {	// falls SD-Karte vorhanden
		f16_printf_P(logStatus.logfile,PSTR("%2i;%2i:%2i:%2i;alt;%c%c%c;%2i;"),
							TM_DD,TM_hh,TM_mm,TM_ss,
							(uint8_t)(anlagenSollStatus.Schalter1 + '0'),
							(uint8_t)(anlagenSollStatus.Schalter2 + '0'),
							(uint8_t)(anlagenSollStatus.Schalter3 + '0'),
							anlagenSollStatus.TReglerWert);

		f16_printf_P(logStatus.logfile,PSTR("neu;%c%c%c;%2i;\r\n"),
							(uint8_t)(aktSoll->Schalter1 + '0'),
							(uint8_t)(aktSoll->Schalter2 + '0'),
							(uint8_t)(aktSoll->Schalter3 + '0'),
							aktSoll->TReglerWert);

		f16_flush();	// Cache leeren
	}
	#endif

	// neue Werte merken
	anlagenSollStatus.Schalter1		= aktSoll->Schalter1;
	anlagenSollStatus.Schalter2		= aktSoll->Schalter2;
	anlagenSollStatus.Schalter3		= aktSoll->Schalter3;
	anlagenSollStatus.TReglerWert 	= aktSoll->TReglerWert;

}
#endif

/**
 *	\ingroup messen
 * SD-Karte initialisieren und Logdatei auf Karte suchen
 */
void log_init()
{
	#if USE_LOGDATEI
	logStatus.logTag	= TM_DD;
	logStatus.logMonat	= TM_MM;
	logStatus.logJahr	= TM_YY;
	sprintf_P((char *)logStatus.log_datei,PSTR("lg%2.2i%2.2i%2.2i.csv"),logStatus.logJahr,logStatus.logMonat,logStatus.logTag);
	MES_DEBUG("\r\nlog_init: %s",logStatus.log_datei);

	if (logStatus.logfile) {
		f16_close(logStatus.logfile);
		logStatus.logfile = 0;
	}
	if (cwdir_ptr) {	// falls SD-Karte vorhanden
		logStatus.logfile = f16_open(logStatus.log_datei,"a");
		f16_fputs_P(PSTR("Log gestartet --------\r\n"),logStatus.logfile);
		f16_flush();	// Cache leeren
	}
	#endif
	return;
}


/**
 *	\ingroup messen
 *	Status mit allen Werten in Logdatei schreiben
 */
void log_status(void)
{
	#if USE_LOGDATEI
	if (logStatus.logfile) {
		f16_printf_P(logStatus.logfile,PSTR("%2i;%2i:%2i:%2i; S;%c%c%c;%2i;%i;%i;%i;"),
							TM_DD, TM_hh, TM_mm, TM_ss,
							(uint8_t)(anlagenStatus.relais1 + '0'),(uint8_t)(anlagenStatus.relais2 + '0'),
							(uint8_t)(anlagenStatus.relais3 + '0'),anlagenStatus.TReglerWert,
							anlagenStatus.Zaehler1,anlagenStatus.Zaehler2,anlagenStatus.Zaehler3);

		// jetzt die Analogwerte
		f16_printf_P(logStatus.logfile,PSTR("%4i;%4i;%4i;%4i;"),var_array[0],var_array[1],var_array[2],var_array[3]);

		// und die 1-wire Sensoren falls vorhanden
		#if USE_OW
		for (int i=0; i < MAXSENSORS; ++i) {
			f16_printf_P(logStatus.logfile,PSTR("%4i;"),ow_array[i]);
		}
		#endif

		f16_fputs_P(PSTR("\r\n"),logStatus.logfile);
		f16_flush();	// Cache leeren
	}
	#endif
}
