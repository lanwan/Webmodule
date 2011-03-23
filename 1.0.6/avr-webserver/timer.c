/*----------------------------------------------------------------------------
 Copyright:      W.Wallucks
 				 nach einer Idee von Dario Carluccio
 Version:        01.05.2008
 Description:    Timer/Scheduler Routinen

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
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "config.h"
#include "timer.h"

#include "usart.h"
#include "stack.h"
#include "ntp.h"

/**
 * \addtogroup funcs Hilfsfunktionen
 *
 * @{
 */

/**
 * \addtogroup time Datum und Zeit
 *	Funktionen zur Datums- und Zeitanzeige
 */

 /**
 * \addtogroup schedul Scheduler
 *	Funktionen zur Zeitsteuerung von Aktionen
 */

/**
 * \file
 * timer.c
 *
 * \author W.Wallucks
 */

/**
 * @}
 */

//#define TIMER_DEBUG	usart_write 
#define TIMER_DEBUG(...)	

/*
 * 	Vars
 */
volatile unsigned long time;	//!< aktuelle Zeit in Sekunden seit 1.1.1900
volatile unsigned int  tmcount = (1000/TIMERBASE);	//!< Zähler um im Interrupt auf 1 Sekunde zu zählen	
volatile unsigned long time_watchdog = 0;

volatile uint8_t TM_hh;		// Uhrzeit
volatile uint8_t TM_mm;
volatile uint8_t TM_ss;
volatile uint8_t TM_DD;		// Datum
volatile uint8_t TM_MM;
volatile uint8_t TM_YY;
volatile uint8_t TM_DOW;	// Wochentag - 0:Sonntag, 1:Montag ...
		 uint8_t TM_DS;

#if USE_SCHEDULER || DOXYGEN
/**
 *	\ingroup schedul
 */
volatile TM_Aktion	TM_Schaltzeit[TM_MAX_SCHALTZEITEN];
#endif

uint8_t Monatstage[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
char	US_Monate[]  PROGMEM = "JanFebMarAprMayJunJulAugSepOctNovDec";

/*
 *	Messintervalle und Zähler
 */
volatile uint16_t timerT;		//!< Messintervall Temperatur

/*
 * 	prototypes
 */
uint8_t AnzahlTageImMonat(uint8_t, uint8_t);
void    TM_SetDayOfWeek(void);   // calc day of week (TM_DD, TM_MM, TM_YY)

//----------------------------------------------------------------------------
/**
 *	\ingroup time
 * Diese Routine startet und initialisiert den Timer
 */
void timer_init (void)
{
	// bei interner Uhr wird Timer1 verwendet
	TCCR1B |= (1<<WGM12) | (1<<CS10 | 0<<CS11 | 1<<CS12);
	TCNT1 = 0;
	// der Compare-Interrupt wird alle 'TIMERBASE' Millisekunden ausgelöst
	OCR1A = (F_CPU / 1024 / 1000 * TIMERBASE)  - 1;
	TIMSK |= (1 << OCIE1A);		// enable overflow interrupt
	machineStatus.Timer2_func = NULL;
	machineStatus.Timer3_func = NULL;
	
	#if USE_SCHEDULER
	for (uint8_t i=0; i<TM_MAX_SCHALTZEITEN; i++){
			TM_Schaltzeit[i].Uhrzeit = TM_SCHALTER_AUS;	 // alle ausschalten, sonst gehen sie um 00:00 Uhr an
	}

	// ersten Eintrag auf Montag 0:00 Uhr mit Defaultwerten besetzen
	TM_Schaltzeit[0].Uhrzeit = 0;
	TM_Schaltzeit[0].Wochentag = 0;
	TM_Schaltzeit[0].Zustand.Schalter1 = 0;
	TM_Schaltzeit[0].Zustand.Schalter2 = 0;
	TM_Schaltzeit[0].Zustand.Schalter3 = 1;
	TM_Schaltzeit[0].Zustand.TReglerWert = 0;
	#endif

    // initial time 00:00:00
    TM_hh = 0;
    TM_mm = 0;
    TM_ss = 0;
    // initial date 1.01.2008
    TM_DD = 1;
    TM_MM = 1;
    TM_YY = 8;
    // day of week
    TM_SetDayOfWeek();
	TM_DS = 0;


	// Timer auf Anfangswerte
	timerT = TIME_TEMP;		// Messintervall Temperatur
	return;
};

/*
 *	\ingroup time
 *	AnzahlTageImMonat
 */
uint8_t AnzahlTageImMonat(uint8_t monat, uint8_t jahr)
{
    if (monat != 2) {
	    return pgm_read_byte(&Monatstage[monat-1]);
    }
	else {
        if ((jahr % 4) != 0) {	// bis zum nächsten Jahrhundertwechsel reicht das!
            return 28;
        }
		else {
            return 29;
        }
    }
}

/**
 *	\ingroup time
 *   GetYearYYYY
 */
uint16_t GetYearYYYY(void)
{
    return 2000 + (uint16_t) TM_YY;
}


/**
 *	\ingroup time
 *   GetUSdate gibt Datum kompatibel mit Internet Datumsangabe zurück
 */
char *GetUSdate(char *datestring)
{
	char month[4];
	strncpy_P(month,&US_Monate[(TM_MM-1)*3],3);
	month[3] = '\0';
	sprintf_P(datestring,PSTR("%2d %s %4d"),TM_DD,month,GetYearYYYY());
	return datestring;
}

/**
 *	\ingroup time
 *	get_datetime
 */

void get_datetime(uint16_t* year, uint8_t* month, uint8_t* day, uint8_t* hour, uint8_t* min, uint8_t* sec)
{
	*year 	= TM_YY + 2000;
	*month 	= TM_MM;
	*day 	= TM_DD;

	*hour	= TM_hh;
	*min	= TM_mm;
	*sec	= TM_ss;
}

/*
 *   
 */
void TM_SetDayofYear(uint16_t tage)
{
	uint8_t i;

	TIMER_DEBUG("\r\nSetDayofYear: %i Tage",tage);
	TM_MM = 1;				// Monate einzeln hochzählen und #Tage im Monat abziehen
	i = AnzahlTageImMonat(TM_MM, TM_YY);
	TIMER_DEBUG("\r\nMonat #%i: %i Tage - Rest: %i -Start",TM_MM,i,tage);
	while ( tage > i ) {
		tage -= i;
		TM_MM += 1;
		i = AnzahlTageImMonat(TM_MM, TM_YY);
		TIMER_DEBUG("\r\nMonat #%i: %i Tage - Rest: %i",TM_MM,i,tage);
	}
	TM_DD = tage;
    TM_SetDayOfWeek();
}

/*
 *   
 */
void TM_SetDayOfWeek(void)
{
    uint16_t day_of_year;
    uint16_t tmp_dow;

    // Day of year
    day_of_year = 31 * (TM_MM-1) + TM_DD;
    
    // Monate kleiner 31 Tage abziehen
    if (TM_MM > 2) {
        if ( TM_YY % 4 ){	// bis zum nächsten Jahrhundertwechsel reicht das!
            day_of_year -= 3;
        } else {
            day_of_year -= 2;
        }
    }
    if (TM_MM >  4) {day_of_year--;}       // april
    if (TM_MM >  6) {day_of_year--;}       // juni
    if (TM_MM >  9) {day_of_year--;}       // september
    if (TM_MM > 11) {day_of_year--;}       // november

    // calc weekday
    tmp_dow = TM_YY + ((TM_YY-1) / 4) - ((TM_YY-1) / 100) + day_of_year;
    if (TM_YY > 0) {
        tmp_dow++;                          // 2000 war Schaltjahr
    }

    // set DOW
    TM_DOW = (uint8_t) ((tmp_dow + 5) % 7);	// 31.12.1999 war Freitag (+5)
}

/**
 *	\ingroup time
 *  TM_AddOneSecond zählt die aktuelle Zeit weiter
 *	und entscheidet auf weiter durchzuführende Aktionen
 */
void TM_AddOneSecond(void)
{
	if (++TM_ss == 60) {
		TM_ss = 0;
		if (++TM_mm == 60) {
			TM_mm = 0;
			// add one hour
			if (++TM_hh == 24) {
				TM_hh = 0;
				TM_AddOneDay();
			}

			// Start der Sommerzeit: März, 2:00:00 ?
			if ((TM_MM==3) && (TM_DOW==0) && (TM_hh==2)) {
                if (TM_DD >= 25 ){		// letzte 7 Tage im Monat ?
                    TM_hh++; 			// 2:00 -> 3:00
                }
            }
			// Ende der Sommerzeit: Oktober, 03:00
			if ((TM_MM==10)  && (TM_DOW==0) && (TM_hh==3) && (TM_DS==0)){
                if (TM_DD >= 25){		// letzte 7 Tage im Monat ?
                    TM_hh--; 			// 3:00 -> 2:00
                    TM_DS=1;			// Marker, dass schon umgestellt ist
                }
			}
        }

		#if USE_SCHEDULER
        // Timer check every 10 minutes
        if ((TM_mm % 10) == 0){
			machineStatus.regeln = 1;
        }
		#endif
 	}

	if (--timerT == 0) {		// Temperaturen messen
		machineStatus.Tlesen = 1;
		timerT = TIME_TEMP;
	}


    if((time_watchdog++) > WTT)
        {
        time_watchdog = 0;
        stack_init();
        }

	#if USE_NTP
	ntp_timer--;
	#endif //USE_NTP

}


/*
 *   
 */
void TM_AddOneDay(void)
{
    // How many days has actual month
    uint8_t dom = AnzahlTageImMonat(TM_MM, TM_YY);
    if (++TM_DD == (dom+1)) {                   // Next Month
		TM_DD = 1;
		if (++TM_MM == 13) {                    // Next year
			TM_MM = 1;
			TM_YY++;
		}
	}

    // next day of week
    TM_DOW++;
    TM_DOW %= 7;

	#if USE_LOGDATEI
	machineStatus.LogInit = true;		// neue Logdatei initialisieren - wird in Mainloop erledigt
	#endif
}



#if USE_SCHEDULER || DOXYGEN

/**
 *	\ingroup schedul
 *	setzt Schaltzeiten
 */
void TM_SchaltzeitSet(uint8_t slot, uint8_t schaltzeit, uint8_t Wochentag, SOLL_STATUS *ptrZustand)
{
    TM_Schaltzeit[slot].Uhrzeit = schaltzeit;
    TM_Schaltzeit[slot].Wochentag = Wochentag;
    TM_Schaltzeit[slot].Zustand.Schalter1 = ptrZustand->Schalter1;
    TM_Schaltzeit[slot].Zustand.Schalter2 = ptrZustand->Schalter2;
    TM_Schaltzeit[slot].Zustand.Schalter3 = ptrZustand->Schalter3;
    TM_Schaltzeit[slot].Zustand.TReglerWert = ptrZustand->TReglerWert;
}

/**
 *	\ingroup schedul
 *	gibt den zur vorgegebenen Zeit gültigen Sollzustand des Systems zurück
 */
bool TM_ZustandGet(uint8_t acttime, uint8_t actdow, SOLL_STATUS *ptrZustand)
{
    uint8_t i,j;
    uint8_t index = 0;
    uint16_t maxtime = 0;

	uint16_t actTageswert = actdow * 256 + acttime;	// gesuchter Wochentag + Uhrzeit + offset zur Wochentags unterscheidung
	uint16_t tstTageswert;
	TIMER_DEBUG("\r\nTM-Debug GetZustand: Tageswert %i",actTageswert);

    // each timer
    for (i=0; i<TM_MAX_SCHALTZEITEN; i++){
        // check if timer > maxtime and timer < actual time
        if ((TM_Schaltzeit[i].Uhrzeit != TM_SCHALTER_AUS) ) {
			for (j=0; j<7; ++j) {										// alle codierten Wochentage durchsuchen
				if (TM_Schaltzeit[i].Wochentag & (1<<j)) {				// falls Wochentag geschaltet wird
					tstTageswert = TM_Schaltzeit[i].Uhrzeit + 256 * j;

					TIMER_DEBUG("\r\nTM-Debug GetZustand: Testwert %i",tstTageswert);

					if ( (tstTageswert <= actTageswert)
						  && (tstTageswert > maxtime) ) {
						index=i;
						maxtime = tstTageswert;
						TIMER_DEBUG("\r\nTM-Debug GetZustand: neuer Index %i",index);
					}
				}
			}
        }
    }

    ptrZustand->Schalter1 = TM_Schaltzeit[index].Zustand.Schalter1;
    ptrZustand->Schalter2 = TM_Schaltzeit[index].Zustand.Schalter2;
    ptrZustand->Schalter3 = TM_Schaltzeit[index].Zustand.Schalter3;
    ptrZustand->TReglerWert = TM_Schaltzeit[index].Zustand.TReglerWert;
    return true;
}

/**
 *	\ingroup schedul
 *	gibt den aktuell gültigen Sollzustand des Systems zurück
 *
 */
bool TM_SollzustandGetAktuell(SOLL_STATUS *ptrZustand)
{
    uint8_t acttime = (TM_hh*10)+(TM_mm/10);

	TIMER_DEBUG("\r\nTM-Debug GetsollZustand: Zeit %i  Tag %i",acttime, TM_DOW);
	return TM_ZustandGet(acttime, TM_DOW, ptrZustand);
}
#endif

/**
 *	\ingroup time
 * Timer Interrupt
 *
 * hier werden nur Flags gesetzt, die in der Mainloop abgearbeitet werden
 *
 * Der Timerinterrupt ist auf TIMERBASE Millisekunden eingestellt. (Standard 25 msec)
 * Timer3 wird mit diesem Takt runtergezählt. Bei erreichen von Null wird das Flag Time3Elapsed
 * gesetzt. Dadurch eignet sich Timer3 für Verzögerungszeiten, die zu lange sind um eine
 * Warteroutine (delay_ms) zu nutzen. Bei Änderungen an TIMERBASE ist darauf zu achten, dass
 * OCR1A etc. mit sinnvollen Werten geladen werden.
 */
ISR (TIMER1_COMPA_vect)
{
	if ( tmcount-- == 0 ) {

		// eine Sekunde ist vergangen - Zähler neu setzen
		tmcount = (1000/TIMERBASE);

		// Sekunde hochzählen
		++time;
		machineStatus.timeChanged++;

	}

	// Timer3 im TIMERBASE-Takt runterzählen
	if (machineStatus.Timer3) {
			machineStatus.Timer3--;
			if (!machineStatus.Timer3) {	// Zero?
				machineStatus.Time3Elapsed = true;
			}
		}

}
