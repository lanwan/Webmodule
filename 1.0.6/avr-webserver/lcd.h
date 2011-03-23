/*------------------------------------------------------------------------------
 Copyright:      Radig Ulrich  mailto: mail@ulrichradig.de
 Author:         Radig Ulrich
 Remarks:        
 known Problems: none
 Version:        22.11.2007
 Description:    Programm zur Ansteuerung eines Standart LCD
				 (HD44870),(SED1278F) und kompatible

Addons :  USE_LCD_4Bit wird zur Ansteuerung für Paraell Ansteuerung benutzt (Ralf Figge)

------------------------------------------------------------------------------*/

/**
 * \addtogroup lcd LCD Anschluss
 *
 * @{
 */

/**
 * \file
 * lcd.c
 *
 * \author Ulrich Radig
 */

/**
 * @}
 */

#if USE_SER_LCD || DOXYGEN
#ifndef _LCD_H_
	#define _LCD_H_

	//Prototypes
	extern void lcd_write (char,char);
#if USE_LCD_4Bit
	extern char Read_LCD (char);
#else
	void shift_data_out (char);
#endif //USE_LCD_4Bit
	extern void lcd_init (void);
	extern void lcd_clear (void);
	extern void lcd_print_P (unsigned char,unsigned char,const char *Buffer,...);
	extern void lcd_print_str (char *Buffer);
	
	#define lcd_print(a,b,format, args...)  lcd_print_P(a,b,PSTR(format) , ## args)
	
	extern volatile unsigned char back_light;
	
	//Anzahl der Zeilen 1,2 oder 4
	//#define ONE_LINES					
	#define TWO_LINES					
	//#define THREE_LINES					
	//#define FOUR_LINES					

/**
 * \ingroup lcd
 *
 * \image html LCD_SER.png "serieller Anschluss eines LCD-Display"
 *
 * \port
 *	Im 4Bit Mode: (USE_LCD_4Bit == 1)
 *	- PortC Pin0 - LCD D4
 *	- PortC Pin1 - LCD D5
 *	- PortC Pin2 - LCD D6
 *	- PortC Pin3 - LCD D7
 *	- PortC Pin4 - LCD RS
 *	- PortC Pin5 - LCD RW
 *	- PortC Pin6 - LCD Enable
 *	- PortC Pin7 - LCD Busybit
 * <br><br>
 *	Im seriellen Mode: (USE_LCD_4Bit == 0)
 *	- PortC Pin2 - LCD Enable
 *	- PortC Pin3 - 74AC164 Data
 *	- PortC Pin4 - 74AC164 Clock
 *
 */
#if USE_LCD_4Bit
	//LCD_D0 - LCD_D3 connect to GND
	//Im 4Bit Mode LCD_D4-->PORTx.0 ........ LCD_D7-->PORTx.3
	//LCD_RS --> PORTx.4 | LCD_RW --> PORTx.5 | LCD_E --> PORTx.6 | PORTx.7-->NotConnect
	#define LCD_Port_DDR			DDRC	//Port an dem das Display angeschlossen wurde
	#define LCD_Port_Write			PORTC
	#define LCD_Port_Read			PINC

	#define LCD_RS					4 		//Pin für RS
	#define LCD_RW					5		//Pin für Read/Write
	#define LCD_E					6 		//Pin für Enable

	#define LCD_DataOutput			0x0f
	#define LCD_DataInput			0x00

	#define BUSYBIT					7
#else	
	#define PORT_LCD_DATA_ENABLE	PORTC
	#define DDR_LCD_DATA_ENABLE		DDRC
	#define LCD_DATA_ENABLE			2
	
	#define PORT_LCD_CLOCK			PORTC
	#define DDR_LCD_CLOCK			DDRC
	#define LCD_CLOCK				4
	
	#define PORT_LCD_DATA			PORTC
	#define DDR_LCD_DATA			DDRC
	#define LCD_DATA				3
	

	
	#define LCD_RS_PIN 				0
	#define LCD_LIGHT_PIN			7

#endif //USE_LCD_4Bit

	#define BUSY_WAIT				6000
	
	#define NOP()	asm("nop")
	#define WAIT(x) for (unsigned long count=0;count<x;count++){NOP();}

#endif //_LCD_H_
#endif //USE_SER_LCD
