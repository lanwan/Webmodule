/*----------------------------------------------------------------------------
 Copyright:      Radig Ulrich  mailto: mail@ulrichradig.de
 Author:         Radig Ulrich
 Remarks:
 known Problems: none
 Version:        24.10.2007
 Description:    RS232 Routinen

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
 * \addtogroup usart
 *
 */

 /**
 * \file
 * usart.c
 *
 * \author Ulrich Radig
 */

/**
 * @}
 */

#ifndef _UART_H
	#define _UART_H

	#define USART_ECHO	1		//!< Echo für eingehende Zeichen auf seriellen Schnittstelle

	#define BUFFER_SIZE	50		//!< Größe des seriellen Empfangspuffers

/**
 * \ingroup usart
 *	Flags für serielle Schnittstelle
 */
	typedef struct {
			volatile unsigned char usart_ready:1;
			volatile unsigned char usart_rx_ovl:1;
			volatile unsigned char usart_disable:1; //benötigt für ftp2com
			} USART_STATUS ;

	extern USART_STATUS usart_status;
	extern volatile unsigned char buffercounter;
	extern char usart_rx_buffer[BUFFER_SIZE];
	extern char *rx_buffer_pointer_in;
	extern char *rx_buffer_pointer_out;

	//----------------------------------------------------------------------------

	//Die Quarzfrequenz auf dem Board (in config.h)
	/*
	#ifndef SYSCLK
			#define SYSCLK 16000000UL
	#endif //SYSCLK
	*/

	//Anpassen der seriellen Schnittstellen Register wenn ein ATMega128 benutzt wird
	#if defined (__AVR_ATmega128__)
		#define USR UCSR0A
		#define UCR UCSR0B
		#define UDR UDR0
		#define UBRR UBRR0L
		#define USART_RX USART0_RX_vect
	#endif

	#if defined (__AVR_ATmega644__) || (defined (__AVR_ATmega644P__) && !USART_USE1)
		#define USR UCSR0A
		#define UCR UCSR0B
		#define UBRR UBRR0L
		#define EICR EICRB
		#define TXEN TXEN0
		#define RXEN RXEN0
		#define RXCIE RXCIE0
		#define UDR UDR0
		#define UDRE UDRE0
		#define USART_RX USART0_RX_vect
	#endif

	#if defined (__AVR_ATmega644P__) && USART_USE1
		#define USR UCSR1A
		#define UCR UCSR1B
		#define UBRR UBRR1L
		#define EICR EICRB
		#define TXEN TXEN1
		#define RXEN RXEN1
		#define RXCIE RXCIE1
		#define UDR UDR1
		#define UDRE UDRE1
		#define USART_RX USART1_RX_vect
	#endif

	#if defined (__AVR_ATmega32__)
		#define USR UCSRA
		#define UCR UCSRB
		#define UBRR UBRRL
		#define EICR EICRB
		#define USART_RX USART_RXC_vect
	#endif

	#if defined (__AVR_ATmega8__)
		#define USR UCSRA
		#define UCR UCSRB
		#define UBRR UBRRL
	#endif

	#if defined (__AVR_ATmega88__)
		#define USR UCSR0A
		#define UCR UCSR0B
		#define UBRR UBRR0L
		#define TXEN TXEN0
		#define UDR UDR0
		#define UDRE UDRE0
	#endif
	//----------------------------------------------------------------------------

	void usart_init(unsigned long baudrate);
	int  usart_putchar(char, FILE *);
	#define usart_write_char(c)  usart_putchar(c, 0)
	#define usart_write_str(str) puts(str)
	#define usart_write(format, args...)   printf_P(PSTR(format) , ## args)

	int usart_getchar(FILE *);

	//----------------------------------------------------------------------------

#endif //_UART_H
