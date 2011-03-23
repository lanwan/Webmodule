/**
 * \addtogroup rc5 Infrarot RC5 Decoder
 *
 * @{
 */

/**
 * \file rc5.h
 *
 * @{
 */

#ifndef _RC5_H_
	#define _RC5_H_

/**
*	Zuordnungen für den von der IR-Fernsteuerung
*	gesendeten Code
*/
#define RC5KEY_0	0	//!< IR Taste 0
#define RC5KEY_1	1	//!< IR Taste 1
#define RC5KEY_2	2	//!< IR Taste 2
#define RC5KEY_3	3	//!< IR Taste 3
#define RC5KEY_4	4	//!< IR Taste 4
#define RC5KEY_5	5	//!< IR Taste 5
#define RC5KEY_6	6	//!< IR Taste 6
#define RC5KEY_7	7	//!< IR Taste 7
#define RC5KEY_8	8	//!< IR Taste 8
#define RC5KEY_9	9	//!< IR Taste 9
#define RC5KEY_10	10	//!< IR Taste 10

#define RC5KEY_OW	13	//!< IR Taste: 1-wire sensor neu lesen
#define RC5KEY_UP	16	//!< IR Taste +
#define RC5KEY_DWN	17	//!< IR Taste -


	/*
	*	Variable
	*/
	extern uint16_t	rc5_data;

	/*
	*	globale Prototypen
	*/
	void rc5_init(void);

#endif
/**
 * @}
 * @}
 */
