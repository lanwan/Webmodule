/*------------------------------------------------------------------------------
 Copyright:      Radig Ulrich + Wilfried Wallucks
 Author:         Radig Ulrich + Wilfried Wallucks
 Remarks:        
 known Problems: none
 Version:        14.05.2008
 Description:    Befehlsinterpreter
------------------------------------------------------------------------------*/

/**
 * \addtogroup tcpsrv	TCP-Service f�r Telnet CMD-Line und FTP
 *
 * @{
  */

/**
 * \file
 * tcpcmd.h
 * CMD Interpreter
 *
 * \author W.Wallucks
 */

/**
 * \addtogroup tcpcmdhelper Hilfs-Funktionen f�r CMD-Interpreter
 *	Hilfs-Funktionen f�r CMD-Interpreter
 */

/**
 * \addtogroup tcpcmd Befehls-Funktionen
 *	Funktionen und Befehlsfunktionen um Befehle
 *	von USART und �ber TCP auszuf�hren
 */

/**
 * @}
 */

#ifndef _TCPCMD_H_
	#define _TCPCMD_H_	
	
	#define HELPTEXT	1		// Helptext anzeigen
	#define MAX_VAR		5
	
	//--------------------------------------------------------

	/**	
	 * \ingroup tcpcmd
	 * Prototyp f�r Befehlsfunktion
	 *
	 * \param[out] Zeiger auf Ausgabepuffer
	 * \returns (String-)L�nge des Ausgabepuffers
	 */
	typedef int16_t(*cmd_fp)(char *);

	/**
	 * \ingroup tcpcmd
	 * Struktur der Tabelle der verarbeiteten Befehle
	 *
	 * Die Tabelle muss mit 2 Nullpointern abgeschlossen werden.
	 */
	typedef struct
	{
		const char* cmd;		//!< Stringzeiger auf Befehlsnamen
		cmd_fp		fp;			//!< Zeiger auf auszuf�hrende Funktion
	} CMD_ITEM;	
	
	/*
	*	Variable
	*/
	extern char *argv;				//!< zeigt auf den Parameterteil des TCP-/USART-Kommandos
	extern CMD_ITEM CMD_TABLE[];	//!< die Tabelle mit CMD-Text/Funktionsaufruf Zuordnungen

	extern cmd_fp RC5_TABLE[];		//!< die Tabelle mit Funktionsaufrufen f�r IR-Bedienung
	extern uint8_t RC5maxcmd;
	/*
	*	globale Prototypen
	*/
	extern unsigned char extract_cmd(char *);
	extern int16_t	cmd_MMCdir(char *);
	
#endif //_TCPCMD_H_


