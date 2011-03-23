#ifndef _TCPSRVD_H
	#define _TCPSRVD_H
 
/**
 * \addtogroup tcpsrv	TCP-Service für Telnet CMD-Line und FTP
 *
 * @{
  */

/**
 * \file
 * tcpsrv.h
 *
 * \author W.Wallucks
 */

/**
 * \addtogroup tcpservice
 *	Hilfs-Funktionen für TCP-Service
 */

/**
 * @}
 */

	//#define TCPSRV_DEBUG	usart_write		//mit Debugausgabe
	#define TCPSRV_DEBUG(...)

#define MAXCMDBUFFER	7		//!< max. Länge eines TCP Kommandos
#define MAXARGVBUFFER	31		//!< max. Länge des Arguments zu einem TCP Kommando

typedef struct {
		volatile uint8_t ctl_waitack	: 1;
		volatile uint8_t data_waitack	: 1;
		volatile uint8_t send_226		: 1;
		volatile uint8_t read_argv		: 1;	// wird cmd selbst oder Parameter gelesen
		volatile uint8_t data_state		: 2;	// 0 : idle; 1 : LIST; 2 : RETR; 3: STOR/Append
		volatile uint8_t transfermode	: 1;	// ASCII (0) oder BINARY (1)
		volatile uint8_t loginOK		: 1;	// Zustand, falls kein anonymer Zugriff erlaubt
		volatile uint8_t userOK			: 1;
		volatile uint8_t LISTcontinue	: 1;	// Flag für LIST cmd
		volatile uint8_t tcpindex;				// Zeiger auf aktuellen TCP Eintrag
		volatile uint8_t cmdindex;				// Zeiger auf aktuelles Zeichen im Puffer
		volatile uint8_t argvindex;				// Zeiger auf aktuelles Zeichen im Puffer
		volatile unsigned char cmdbuffer[MAXCMDBUFFER+1];	// Puffer für Kommando
		volatile unsigned char argvbuffer[MAXARGVBUFFER+1];	// Puffer für Parameter
		unsigned char 	fname[32];				// long filename
		#if USE_MMC
		File 			*datafile;				// 
		uint32_t		filesize;
		uint32_t		charcount;				// Anzahl der bereits gesendeten Character
		uint32_t		old_charcount;			// Anzahl der vor dem letzten Paket gesendeten Character
		#endif
} TCPSRV_STATUS;

	/*
	*	Variable
	*/
	extern TCPSRV_STATUS tcpsrv_status;

	/*
	*	globale Prototypen
	*/
	void 	 tcpsrvd_init (void);
	uint16_t doSrvCmd(uint8_t, unsigned char *);
	void 	 tcpsrvd (uint8_t);
	void 	 send_data_status(unsigned char);
 
#endif

