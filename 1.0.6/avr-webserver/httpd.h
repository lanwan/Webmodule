/*----------------------------------------------------------------------------
 Copyright:      Radig Ulrich  mailto: mail@ulrichradig.de
 Author:         Radig Ulrich
 Remarks:        
 known Problems: none
 Version:        24.10.2007
 Description:    Webserver Applikation
 
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
 * \addtogroup webserver
 *
 *
 * @{
 */

/**
 * \file
 * httpd.c
 *
 * \author Ulrich Radig & W.Wallucks
 */

#ifndef _HTTPD_H
	#define _HTTPD_H
	
    #define HTTPD_PORT				80
	#define CONVERSION_BUFFER_LEN	16
	
	typedef struct
	{
		const char *filename;		//!< Dateiname der Seite
		PGM_P page_pointer; 	 	//!< Zeiger auf Speicherinhalt
	} WEBPAGE_ITEM;
	
	struct http_table
	{
		PGM_P old_page_pointer					;
		PGM_P new_page_pointer					;
		unsigned char *auth_ptr 				;
		unsigned char *hdr_end_pointer			;
		unsigned char http_auth 		: 1		;
		unsigned char http_header_type	: 2		;
		unsigned char first_switch		: 1		;
		unsigned char post				: 1		;
		unsigned char *post_ptr					;
		unsigned char *post_ready_ptr			;
		#if USE_CAM
		unsigned char cam				: 2		;
		#endif //USE_CAM
		#if USE_MMC
		unsigned char 	mmc				: 1;	//!< Flag, falls Datei auf SD/MMC-Karte
		unsigned char 	fname[13];
		long			filesize;
		long			charcount;				//!< Anzahl der bereits gesendeten Character
		long			old_charcount;			//!< Anzahl der vor dem letzten Paket gesendeten Character
		#endif //USE_MMC
	};
	
	//Prototypen
	void httpd (unsigned char);
	void httpd_init (void);
	void httpd_stack_clear (unsigned char);
	void httpd_header_check (unsigned char);
	void httpd_data_send (unsigned char);
 
#endif //_HTTPD_H


/**
 * @}
 */
