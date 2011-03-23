/*----------------------------------------------------------------------------
 Copyright:      W.Wallucks
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
/**
 * \addtogroup funcs Hilfsfunktionen
 *
 * @{
 */

/**
 * \file
 * timer.h
 *
 * \author W.Wallucks
 */

/**
 * @}
 */
#ifndef _TIMER_H
	#define _TIMER_H

	#if defined (__AVR_ATmega644__) || defined (__AVR_ATmega644P__)
		#define TIMSK TIMSK1
	#endif

    #define WTT 1200 //!< Watchdog Time

	#define	TM_SCHALTER_AUS	0xff	//!< Schaltzeit wird nicht verwendet

	extern volatile unsigned long time;
	extern volatile unsigned long time_watchdog;
 
	extern 			char	US_Monate[];
	extern volatile uint8_t TM_hh;
	extern volatile uint8_t TM_mm;
	extern volatile uint8_t TM_ss;
	extern volatile uint8_t TM_DD;
	extern volatile uint8_t TM_MM;
	extern volatile uint8_t TM_YY;
	extern volatile uint8_t TM_DOW;
	extern volatile TM_Aktion	TM_Schaltzeit[];

	void timer_init (void);
	uint16_t GetYearYYYY(void);
	char *GetUSdate(char*);
	void TM_SetDayofYear(uint16_t);
	void TM_AddOneSecond(void);
	void TM_AddOneDay(void);      // add one day to actual date

	void TM_SchaltzeitSet(uint8_t slot, uint8_t schaltzeit, uint8_t Wochentag, SOLL_STATUS *ptrZustand);
	bool TM_ZustandGet(uint8_t acttime, uint8_t actdow, SOLL_STATUS *ptrZustand);
	bool TM_SollzustandGetAktuell(SOLL_STATUS *ptrZustand);

#endif //_TIMER_H

