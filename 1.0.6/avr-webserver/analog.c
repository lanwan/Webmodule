/*------------------------------------------------------------------------------
 Copyright:      Radig Ulrich  mailto: mail@ulrichradig.de
 Author:         Radig Ulrich
 Remarks:        
 known Problems: none
 Version:        31.12.2007
 Description:    Analogeingänge Abfragen
 
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
 * \addtogroup analog ADC Wandler
 *
 *	analoge Messwerte an PortA des AtMega<br>
 *	Aktivierung mittels #USE_ADC in der config.h
 *
 *	Die Messwerte werden im Variablenarray #var_array gespeichert.
 *	(maximal #MAXADCHANNEL Messkanäle)
 *
 */

 /**
 * \file
 * analog.c
 *
 * \author Ulrich Radig
 */

 /**
 * @}
 */

#include "config.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "analog.h"

#if USE_ADC || DOXYGEN
volatile unsigned char channel = 0;	//!< aktueller Messkanal

//------------------------------------------------------------------------------
//
/**
 *	\ingroup analog
 *	Initialisierung<br>
 *	frei laufender AD-Wandler der nach jedem Messvorgang
 *	einen Interrupt auslöst<br>
 *	Teilerfaktor ist auf 128 eingestellt
 */
void ADC_Init(void)
{ 
	ADMUX = (1<<REFS0);
	//Free Running Mode, Division Factor 128, Interrupt on
	ADCSRA=(1<<ADEN)|(1<<ADSC)|(1<<ADATE)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0)|(1<<ADIE);
}

//------------------------------------------------------------------------------
//
/**
 *	\ingroup analog
 *	Interrupt bei beendetem Messvorgang<br>
 *	speichert Messwert und legt den AD-Multiplexer
 *	auf den nächsten freien Port (siehe #MAXADCHANNEL)
 */
ISR(ADC_vect)
{
    ANALOG_OFF; //ADC OFF
	var_array[channel++] = ADC;
	//usart_write("Kanal(%i)=%i\n\r",(channel-1),var_array[(channel-1)]);
	if (channel > (MAXADCHANNEL-1)) channel = 0;
    ADMUX =(1<<REFS0) + channel;
    //ANALOG_ON;//ADC ON
}

#endif //USE_ADC


