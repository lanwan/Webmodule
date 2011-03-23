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

/**
 * \addtogroup messen Datenaufnahme und Logging
 *
 * @{
  */

/**
 * \file
 * Messung.h
 * Definitionen der Ports für angeschlossene Relais und Schalter
 *
 * \author W.Wallucks
 */

/**
 * @}
 */
#ifndef _MESSUNG_H_
	#define _MESSUNG_H_

/**
 * \port
 *	- Port C - An/Aus Sensoren an Pin 0, 1, 6 und 7
 *	- Portausgang mit Schalter an PD4, PD5 und PD6
 */
#define SENS_Read			PINC	//!< Porteingang an dem die An/Aus Sensoren angeschlossen sind
#define SENS_PULLUP			PORTC	//
#define SENS_DDR			DDRC	
#define SENS_INTENA			PCIE2	// == Port C
#define SENS_INTMASK		PCMSK2
#define SENS_PIN1			0		//!< Sensor an Pin 0
#define SENS_PIN2			1		//!< Sensor an Pin 1
#define SENS_PIN3			6		//!< Sensor an Pin 6
#define SENS_PIN4			7		//!< Sensor an Pin 7
#define SENS_ACTIVE_PINS	((1<<SENS_PIN1) | (1<<SENS_PIN2) | (1<<SENS_PIN3) | (1<<SENS_PIN4))


#define PORT_SCHALTER1		PD4		//!< Portausgang mit Schalter
#define PORT_SCHALTER2		PD5		//!< Portausgang mit Schalter
#define PORT_SCHALTER3		PD6		//!< Portausgang mit Schalter
#define OUT_DDR				DDRD
#define OUT_PORT			PORTD


/*
* Schalter1-3 schalten
*/
#define S1An()		OUT_PORT |= (1<<PORT_SCHALTER1)		//!< Port high schalten
#define S1Aus()		OUT_PORT &= ~(1<<PORT_SCHALTER1)	//!< Port low  schalten
#define S1Toggle()	OUT_PORT ^= (1<<PORT_SCHALTER1)		//!< Port umschalten
#define S2An()		OUT_PORT |= (1<<PORT_SCHALTER2)
#define S2Aus()		OUT_PORT &= ~(1<<PORT_SCHALTER2)
#define S2Toggle()	OUT_PORT ^= (1<<PORT_SCHALTER2)
#define S3An()		OUT_PORT |= (1<<PORT_SCHALTER3)
#define S3Aus()		OUT_PORT &= ~(1<<PORT_SCHALTER3)
#define S3Toggle()	OUT_PORT ^= (1<<PORT_SCHALTER3)

#if USE_OW
extern volatile int16_t ow_array[MAXSENSORS];	// Speicherplatz für 1-wire Sensorwerte
#endif

//----------------------------------------------------------------------------
// Prototypen
void messung_init(void);
uint8_t start_OW(void);
void lese_Temperatur(void);
void set_SensorResolution(void);
int16_t initSchaltzeiten(char *);
void regelAnlage(SOLL_STATUS *);
void log_init(void);
void log_status(void);

#endif
