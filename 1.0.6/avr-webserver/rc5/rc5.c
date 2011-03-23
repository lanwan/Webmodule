/************************************************************************/
/*                                                                      */
/*                      RC5 Remote Receiver                             */
/*                                                                      */
/*              Author: Peter Dannegger                                 */
/*                      danni@specs.de                                  */
/*                                                                      */
/************************************************************************/


/**
 * \addtogroup rc5 Infrarot RC5 Decoder
 *
 *	Als IR-Empfänger verwendet man einen TSOP1736, der direkt an 5V und
 *	einen Inputport (RC5_PIN) angeschlossen wird.
 *
 *	Ein Beispiel zur Verwendung der Fernbedienung findet sich in der Mainloop
 * \ref RC5Beispiel "Steuerung mit IR-Fernbedienung". Sollen kurze im
 *  Millisekundenbereich liegende Pulse an Ausgängen erzeugt werden sollte
 *  auch der Abschnitt zu dem \ref t3timer "Countdown-Timer T3" beachtet werden.
 *
 * \image html TSOP1736.png "Anschlussbelegung TSOP17xx"
 *
 * @{
 *
 * \file rc5.c
 *
 * @{
 * \author Peter Dannegger
 * \htmlinclude  INDEXG.HTM
 * 
 */

#include "../config.h"

#include <stdint.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>


#if USE_RC5

//#include "rc5.h"

#define RC5TIME 	1.778e-3		// 1.778msec
#define PULSE_MIN	(uint8_t)(F_CPU / 512 * RC5TIME * 0.4 + 0.5)
#define PULSE_1_2	(uint8_t)(F_CPU / 512 * RC5TIME * 0.8 + 0.5)
#define PULSE_MAX	(uint8_t)(F_CPU / 512 * RC5TIME * 1.2 + 0.5)

#define	xRC5_IN		RC5_INPORT
#define	xRC5		RC5_PIN			// IR input low active


uint8_t		rc5_bit;				// bit value
uint8_t		rc5_time;				// count bit time
uint16_t	rc5_tmp;				// shift bits in
uint16_t	rc5_data;				// store result


void rc5_init()
{
	rc5_data = 0;

	RC5_DDR &= ~(1<<RC5_PIN);		// Pin auf Eingang
	RC5_INPORT |= (1<<RC5_PIN);		// Pullup

	TCCR0B = 1<<CS02;				//divide by 256
	TIMSK0 = 1<<TOIE0;				//enable timer interrupt
}


//SIGNAL (SIG_OVERFLOW0)
ISR(TIMER0_OVF_vect )
{
  uint16_t tmp = rc5_tmp;						// for faster access

 	TCNT0 = -2;									// 2 * 256 = 512 cycle

 	if( ++rc5_time > PULSE_MAX ){				// count pulse time
    	if( !(tmp & 0x4000) && tmp & 0x2000 )	// only if 14 bits received
      	rc5_data = tmp;
    	tmp = 0;
  	}

  	if( (rc5_bit ^ xRC5_IN) & 1<<xRC5 ){		// change detect
    	rc5_bit = ~rc5_bit;						// 0x00 -> 0xFF -> 0x00

    	if( rc5_time < PULSE_MIN )				// to short
      		tmp = 0;

    	if( !tmp || rc5_time > PULSE_1_2 ){		// start or long pulse time
      		if( !(tmp & 0x4000) )				// not to many bits
        	tmp <<= 1;							// shift	
      		if( !(rc5_bit & 1<<xRC5) )			// inverted bit
        		tmp |= 1;						// insert new bit
      		rc5_time = 0;						// count next pulse time
    	}
  	}

  	rc5_tmp = tmp;
}

#endif
/**
 * @}
 * @}
 */
