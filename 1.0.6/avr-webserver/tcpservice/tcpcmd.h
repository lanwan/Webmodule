/*------------------------------------------------------------------------------
 Copyright:      Radig Ulrich + Wilfried Wallucks
 Author:         Radig Ulrich + Wilfried Wallucks
 Remarks:        
 known Problems: none
 Version:        14.05.2008
 Description:    Befehlsinterpreter
------------------------------------------------------------------------------*/

/**
 * \addtogroup tcpsrv	TCP-Service für Telnet CMD-Line und FTP
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
 * \addtogroup tcpcmdhelper Hilfs-Funktionen für CMD-Interpreter
 *	Hilfs-Funktionen für CMD-Interpreter
 */

/**
 * \addtogroup tcpcmd Befehls-Funktionen
 *	Funktionen und Befehlsfunktionen um Befehle
 *	von USART und über TCP auszuführen
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
	 * Prototyp für Befehlsfunktion
	 *
	 * \param[out] Zeiger auf Ausgabepuffer
	 * \returns (String-)Länge des Ausgabepuffers
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
		cmd_fp		fp;			//!< Zeiger auf auszuführende Funktion
	} CMD_ITEM;	
	
	/*
	*	Variable
	*/
	extern char *argv;				//!< zeigt auf den Parameterteil des TCP-/USART-Kommandos
	extern CMD_ITEM CMD_TABLE[];	//!< die Tabelle mit CMD-Text/Funktionsaufruf Zuordnungen

	extern cmd_fp RC5_TABLE[];		//!< die Tabelle mit Funktionsaufrufen für IR-Bedienung
	extern uint8_t RC5maxcmd;
	/*
	*	globale Prototypen
	*/
	extern unsigned char extract_cmd(char *);
	extern int16_t	cmd_MMCdir(char *);
	
#endif //_TCPCMD_H_


