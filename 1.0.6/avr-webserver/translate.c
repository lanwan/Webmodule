/*
 * Copyright (c) 2008 by W.Wallucks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <string.h>
#include <avr/pgmspace.h>

#include "timer.h"
#if USE_OW
#include "messung.h"
#endif

#include "usart.h"
//#define FUNCS_DEBUG usart_write	//mit Debugausgabe
#define FUNCS_DEBUG(...)		//ohne Debugausgabe


/**
 * \addtogroup funcs Hilfsfunktionen
 *
 * @{
  */

/**
 * \addtogroup allgfunktionen allgemeine Hilfsfunktionen
 *
 */

/**
 * \file
 translate.h
 *
 * \author W.Wallucks
 */

/**
 * @}
 */

char Tagesnamen[] PROGMEM = "SonMonDieMitDonFreSam";	// 0:Sonntag

/**
 *	\ingroup allgfunktionen
 *	Zeile nach Parameter durchsuchen und ersetzen
 *
 *	Mögliche Variable haben ein Prozentzeichen (\%) vorangestellt und sind:
 *	\li \b DATE	Tagesdatum
 *	\li \b USDATE	Tagesdatum im Internet kompatiblen Format
 *	\li	\b WDAY	Wochentag, abgekürzt auf 3 Buchstaben
 *	\li \b TIME	aktuelle Zeit
 *	\li \b VA\@nn	analoger Wert nn
 *	\li \b OW\@nn	nn = 00 bis MAXSENSORS-1 gibt 1-Wire Temperaturwerte in 1/10 °C aus
 *	\li \b OW\@mm	mm = 20 bis MAXSENSORS-1+20 gibt 1-Wire Temperaturwerte in °C mit einer Nachkommastelle aus<br>
 *	d.h. OW\@nn für Balkenbreite verwenden und OW\@mm für Celsius-Anzeige
 *	\li \b PORTnm	Status des Ausgangs Pin m (0, 1, ...) an Port n (A, B, C, ...)
 *	\li \b PINnm	Status des Eingangs Pin m (0, 1, ...) an Port n (A, B, C, ...)<br>
 *		 dargestellt durch eine jpg-Datei ledon.jpg / ledoff.jpg
 *
 *	\param[in] buffer String mit eingebetteten Parametern
 *	\param[in,out] ptr Speicherplatz des konvertierten Strings
 *	\param[out] nbytes Anzahl verarbeiteter Bytes
 *	\returns Anzahl der Bytes im konvertierten String
*/
uint16_t translate(char *buffer, char **ptr, uint16_t *nbytes )
{
	uint16_t len = 0;
	char *src = buffer;
	char *dest = *ptr;

	while (*src) {
		
		if (*src != '%') {
			*dest++ = *src++;
			++len;
		}
		else {
			++src;

			if (strncasecmp_P(src,PSTR("TIME"),4)==0) {
				uint16_t year;
				uint8_t month, day, hour, min, sec;
				FUNCS_DEBUG(" - Zeit");
				get_datetime(&year, &month, &day, &hour, &min, &sec);
				sprintf_P(dest,PSTR("%2.2d:%2.2d:%2.2d"),hour,min,sec);
				src += 4;
			}

			else if (strncasecmp_P(src,PSTR("DATE"),4)==0) {
				uint16_t year;
				uint8_t month, day, hour, min, sec;
				FUNCS_DEBUG(" - Datum");
				get_datetime(&year, &month, &day, &hour, &min, &sec);
				sprintf_P(dest,PSTR("%2.2d.%2.2d.%4d"),day,month,year);
				src += 4;
			}

			else if (strncasecmp_P(src,PSTR("USDATE"),6)==0) {
				FUNCS_DEBUG(" - USDatum");
				GetUSdate(dest);
				src += 6;
			}

			else if (strncasecmp_P(src,PSTR("WDAY"),4)==0) {
				FUNCS_DEBUG(" - Wochentag");
				memcpy_P(dest,&Tagesnamen[TM_DOW*3],3);
				dest += 3;
				*dest = '\0';
				src += 4;
			}

			#if USE_ADC
			else if (strncasecmp_P(src,PSTR("VA@"),3)==0) {	
				FUNCS_DEBUG(" - Analogwert");
				uint8_t i = (*(src+3)-48)*10 + (*(src+4)-48);
				itoa (var_array[i],dest,10);
				src += 5;
			}
			#endif

			#if 0
			//
			// KTY0D gibt Differenzmessung in dezimalen Grad aus
			// KTY0x gibt Wert in 1/10 Grad aus
			// entsprechend KTY1D/KTY1x für 2. Differenzmessung
			//
			else if (strncasecmp_P(src,PSTR("KTY"),3)==0) {	
				FUNCS_DEBUG(" - KTY-Wert");
				b = (*(src+3)=='0')? KTY_SENS1 : KTY_SENS2;	// Speicherplätze der Differenzmessungen

				int16_t T = (int16_t) (var_array[b] * -0.62 + 224.3);

				if (*(src+4)=='D') {
					int8_t j = (int8_t)(T / 10);
					itoa (j,dest,10);

					while (*dest++)			// neues Ende finden
						++len;
					--dest;

					*dest++ = ',';
					++len;

					j = T % 10;				// Nachkommastelle
					itoa (j,dest,10);
				} else if (T < 0) {
					T = -T;					// Vorzeichen abschneiden
					itoa (T,dest,10);

					while (*dest++)			// neues Ende finden
						++len;
					--dest;

					// "style rechtsbündig" anhängen
					strcpy_P(dest,PSTR("\" style=\"float: right"));
				}
				else {
					itoa (T,dest,10);
				}
				src += 5;
			}
			#endif
			
#if USE_OW
			/*
			*	1-Wire Temperatursensoren
			*	-------------------------
			*	OW@nn	nn = 00 bis MAXSENSORS-1 gibt Werte in 1/10 °C aus
			*	OW@mm	mm = 20 bis MAXSENSORS-1+20 gibt Werte in °C mit einer Nachkommastelle aus
			*	d.h. OW@nn für Balkenbreite verwenden und OW@mm für Celsius-Anzeige
			*/
			else if (strncasecmp_P(src,PSTR("OW@"),3)==0) {	
				FUNCS_DEBUG(" - 1-wire");
				uint8_t i = (*(src+3)-48)*10 + (*(src+4)-48);
				if (i >= 20) {	// Offset bei Sensor# abziehen und Wert als Dezimalzahl ausgeben
					i -= 20;
					dtostrf(ow_array[i] / 10.0,3,1,dest);
				} else {
					itoa (ow_array[i],dest,10);
				}
				src += 5;
			}
#endif

			//Einsetzen des Port Status %PORTxy durch "checked" wenn Portx.Piny = 1
			//x: A..G  y: 0..7 
			else if (strncasecmp_P(src,PSTR("PORT"),4)==0) {
				FUNCS_DEBUG(" - Portstatus");
				uint8_t pin  = (*(src+5)-48);	
				uint8_t b = 0;
				switch(*(src+4)) {
					case 'A':
						b = (PORTA & (1<<pin));
						break;
					case 'B':
						b = (PORTB & (1<<pin));
						break;
					case 'C':
						b = (PORTC & (1<<pin));
						break;
					case 'D':
						b = (PORTD & (1<<pin));
						break; 
				}
				
				if(b) {
					//strcpy_P(dest, PSTR("checked"));
					strcpy_P(dest, PSTR("ledon.gif"));
				}
				else {
					//strcpy_P(dest, PSTR("unchecked"));
					strcpy_P(dest, PSTR("ledoff.gif"));
				}

				src += 6;
			}
			
			//Einsetzen des Pin Status %PI@xy bis %PI@xy durch "ledon" oder "ledoff"
			//x = 0 : PINA / x = 1 : PINB / x = 2 : PINC / x = 3 : PIND
			else if (strncasecmp_P(src,PSTR("PIN"),3)==0) {
				FUNCS_DEBUG(" - Eingangswert");
				uint8_t pin  = (*(src+4)-48);	
				uint8_t b = 0;
				switch(*(src+3)) {
					case 'A':
						b = (PINA & (1<<pin));
						break;
					case 'B':
						b = (PINB & (1<<pin));
						break;
					case 'C':
						b = (PINC & (1<<pin));
						break;
					case 'D':
						b = (PIND & (1<<pin));
						break;    
				}
				
				if(b) {	// gesetztes bit bedeutet: nix dran, da Pullup Widerstand
					strcpy_P(dest, PSTR("ledoff.gif"));
				} else {
					strcpy_P(dest, PSTR("ledon.gif"));	// Schalter auf Masse geschlossen
				}

				src += 5;
			}
			else {	// nix gefunden -> '%' speichern
				*dest++ = '%';
				*dest = 0;
				++len;
			}

			while (*dest++)	// neues Ende finden
				++len;
			--dest;
		}

	}
	*ptr = dest;
	*nbytes = (uint16_t)(src - buffer);
	return len;
}
