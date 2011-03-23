/*----------------------------------------------------------------------------
 Copyright:      Radig Ulrich  mailto: mail@ulrichradig.de
 Author:         Radig Ulrich
 Remarks:        
 known Problems: none
 Version:        23.12.2007
 Description:    RS232 Camera Routinen
                 Kamera arbeitet nur mit einem 14,7456Mhz Quarz!
----------------------------------------------------------------------------*/

#if USE_CAM || DOXYGEN	
#ifndef _CAM_H
	#define _CAM_H
	
	//----------------------------------------------------------------------------	
	
	
	#if defined (__AVR_ATmega32__)
		#define DAT_BUFFER_SIZE	200
	#endif

	#if defined (__AVR_ATmega644__) || defined(__AVR_ATmega644P__)
		#if defined USE_MMC	
			#define DAT_BUFFER_SIZE	512
		#else
			#define DAT_BUFFER_SIZE	1200
		#endif
	#endif
	
	#define CMD_BUFFER_SIZE	17
	#define CAM_HEADER 16
	
	volatile unsigned long max_bytes;
	volatile long cam_dat_start;
	volatile long cam_dat_stop;
	volatile long cmd_buffercounter;
	volatile long dat_buffercounter;
	volatile unsigned char cam_cmd_buffer[CMD_BUFFER_SIZE];
	volatile unsigned char cam_dat_buffer[DAT_BUFFER_SIZE];
	
	//Anpassen der seriellen Schnittstellen Register wenn ein ATMega128 benutzt wird
	#if defined (__AVR_ATmega128__)
		#define CAM_USR UCSR0A
		#define CAM_UCR UCSR0B
		#define CAM_UDR UDR0
		#define CAM_UBRR UBRR0L
		#define CAM_USART_RX USART0_RX_vect 
	#endif
	
	#if defined (__AVR_ATmega644__)
		#define CAM_USR UCSR0A
		#define CAM_UCR UCSR0B
		#define CAM_UBRR UBRR0L
		#define CAM_EICR EICRB
		#define CAM_TXEN TXEN0
		#define CAM_RXEN RXEN0
		#define CAM_RXCIE RXCIE0
		#define CAM_UDR UDR0
		#define CAM_UDRE UDRE0
		#define CAM_USART_RX USART0_RX_vect   
	#endif
	
	// CAM an USART1 oder USART0
	#if defined (__AVR_ATmega644P__) || DOXYGEN	
		#if USART_USE1
			/**
			 * \port
			 *	- USART1 ist durch Terminal belegt -> CAM an USART0
			 */
			#define CAM_USR UCSR0A
			#define CAM_UCR UCSR0B
			#define CAM_UBRR UBRR0L
			#define CAM_TXEN TXEN0
			#define CAM_RXEN RXEN0
			#define CAM_RXCIE RXCIE0
			#define CAM_UDR UDR0
			#define CAM_UDRE UDRE0
			#define CAM_USART_RX USART0_RX_vect   
		#else
			/**
			 * \port
			 *	- USART0 ist durch Terminal belegt -> CAM an USART1
			 */
			#define CAM_USR UCSR1A
			#define CAM_UCR UCSR1B
			#define CAM_UBRR UBRR1L
			#define CAM_TXEN TXEN1
			#define CAM_RXEN RXEN1
			#define CAM_RXCIE RXCIE1
			#define CAM_UDR UDR1
			#define CAM_UDRE UDRE1
			#define CAM_USART_RX USART1_RX_vect   
		#endif
	#endif
	
	#if defined (__AVR_ATmega32__)
		#define CAM_USR UCSRA
		#define CAM_UCR UCSRB
		#define CAM_UBRR UBRRL
		#define CAM_EICR EICRB
		#define CAM_USART_RX USART_RXC_vect  
	#endif
	
	#if defined (__AVR_ATmega8__)
		#define CAM_USR UCSRA
		#define CAM_UCR UCSRB
		#define CAM_UBRR UBRRL
	#endif
	
	#if defined (__AVR_ATmega88__)
		#define CAM_USR UCSR0A
		#define CAM_UCR UCSR0B
		#define CAM_UBRR UBRR0L
		#define CAM_TXEN TXEN0
		#define CAM_UDR UDR0
		#define CAM_UDRE UDRE0
	#endif
	//----------------------------------------------------------------------------
	
	void cam_init (void);
	void cam_uart_init(void); 
	void cam_uart_write_char(char);
	void cam_command_send (unsigned char,unsigned char,unsigned char,unsigned char,unsigned char);
	unsigned long cam_picture_store (char mode);
	char cam_data_get (unsigned long byte);
	
	//----------------------------------------------------------------------------

#endif //_CAM_H
#endif //USE_CAM
