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
 * \addtogroup usart serielle Schnittstelle
 *
 *	serielle Ein- und Ausgabe auf Usart
 *
 */

 /**
 * \file
 * usart.c
 *
 * \author Ulrich Radig & W.Wallucks
 */

 /**
 * @}
 */

#include "config.h"

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <stdio.h>


#if CMD_TELNET
#include "telnetd.h"
#include "stack.h"
#endif
#include "usart.h"

volatile unsigned char buffercounter = 0;

char usart_rx_buffer[BUFFER_SIZE];					//!< usart Empfangspuffer
char *rx_buffer_pointer_in	= &usart_rx_buffer[0];	//!< Zeiger auf empfangene Zeichen
char *rx_buffer_pointer_out	= &usart_rx_buffer[0];	//!< Zeiger auf zu sendende Zeichen
USART_STATUS usart_status;							//!< Statusflags für serielle Schnittstelle
	
//----------------------------------------------------------------------------
/**
 *	\ingroup usart
 * Init serielle Schnittstelle
 */
void usart_init(unsigned long baudrate) 
{ 
#if defined (__AVR_ATmega644P__) || !USE_CAM
	//Serielle Schnittstelle 1
  	//Enable TXEN im Register UCR TX-Data Enable
	UCR =(1 << TXEN | 1 << RXEN | 1<< RXCIE);
	// 0 = Parity Mode Disabled
	// 1 = Parity Mode Enabled, Even Parity
	// 2 = Parity Mode Enabled, Odd Parity
	//UCSRC = 0x06 + ((parity+1)<<4);
	//UCSRC |= (1<<USBS);
	//Teiler wird gesetzt 
	UBRR=(F_CPU / (baudrate * 16L) - 1);
	usart_status.usart_disable = 0;
#endif //USE_CAM
}

//----------------------------------------------------------------------------
/**
 *	\ingroup usart
 * Routine für die Serielle Ausgabe eines Zeichens
 */
int usart_putchar(char c, FILE *stream)
{
#if CMD_TELNET
	if(usart_status.usart_disable)
	{
        if(rx_buffer_pointer_in == (rx_buffer_pointer_out - 1))
        {
            //Datenverlust
                    telnetd_send_data ();
                    while(telnetd_status.ack_wait)
                    {
                        eth_get_data();
                    }
        }

        *rx_buffer_pointer_in++ = c;

        if (rx_buffer_pointer_in == &usart_rx_buffer[BUFFER_SIZE-1])
        {
            rx_buffer_pointer_in = &usart_rx_buffer[0];
        }
    }
    return 0;
#else
	#if defined (__AVR_ATmega644P__) || !USE_CAM
	if(!usart_status.usart_disable)
	{
		//Warten solange bis Zeichen gesendet wurde
		while(!(USR & (1<<UDRE)));
		//Ausgabe des Zeichens
		UDR = c;
	}
	return 0;
	#endif //USE_CAM
#endif
}

//----------------------------------------------------------------------------
int usart_getchar(FILE *stream)
{
	if (buffercounter > 0) {
		return usart_rx_buffer[0];
	}
	return 0;
}

//----------------------------------------------------------------------------
#if defined (__AVR_ATmega644P__) || !USE_CAM
/**
 *	\ingroup usart
 * Empfang eines Zeichens
 */
ISR(USART_RX)
{
	if(!usart_status.usart_disable)
	{
		unsigned char receive_char;
		receive_char = (UDR);
		
		#if USART_ECHO
		usart_write_char(receive_char);
		#endif
	
		if (usart_status.usart_ready)
		{
			usart_status.usart_rx_ovl = 1;
			return; 
		}
        
        if (receive_char == 0x08)
        {
            if (buffercounter) buffercounter--;
            return;
        }
    
		if (receive_char == '\r' && (!(usart_rx_buffer[buffercounter-1] == '\\')))
		{
			usart_rx_buffer[buffercounter] = 0;
			buffercounter = 0;
			usart_status.usart_ready = 1;
			return;    
		}
	
		if (buffercounter < BUFFER_SIZE - 1)
		{
			usart_rx_buffer[buffercounter++] = receive_char;    
		}
	}
	else
	{
		if(rx_buffer_pointer_in == (rx_buffer_pointer_out - 1))
		{
			//Datenverlust
			return;
		}
	
		*rx_buffer_pointer_in++ = UDR;
	
		if (rx_buffer_pointer_in == &usart_rx_buffer[BUFFER_SIZE-1])
		{
			rx_buffer_pointer_in = &usart_rx_buffer[0];
		}
	}
	return;
}
#endif //USE_CAM


