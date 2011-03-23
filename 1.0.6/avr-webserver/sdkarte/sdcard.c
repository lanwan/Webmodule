/*----------------------------------------------------------------------------
 Copyright:      W.Wallucks
 known Problems: ?
 Version:        01.04.2008
 Description:    SD-Kartenanbindung an AVR-Miniwebserver

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
----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <avr/pgmspace.h>

#include "../config.h"

#if USE_MMC
#include "partition.h"
#include "fat16.h"
#include "fat16_config.h"
#include "sd_raw.h"
#include "sd.h"
#include "spi.h"
#include "sdcard.h"
#include "../timer.h"


#include "../usart.h"
//#define SD_DEBUG	usart_write 
#define SD_DEBUG(...)	


/**
 * \defgroup sdcard SD/MMC Karte
 *
 * es wird das MMC/SD memory card interface von Roland Riegel genutzt
 *
 *	Die Funktionen aus sdcard.h sollten im Normalfall ausreichen um eine
 *	SD-Karte anzusprechen. Es wurde versucht mit den <b>f16_<tt>xxx</tt></b> Funktionen
 *	die Standardfunktionen der C-Standardbibliothek nachzuempfinden. Die Funktionen
 *	<b>fat16_<tt>xxx</tt></b>, <b>sd_<tt>xxx</tt></b>, <b>spi_<tt>xxx</tt></b> und
 *	<b>partition<tt>xxx</tt></b> sind aus der Bibliothek von Roland Riegel. Sie sind als
 *	"low level"-Funktionen anzusehen.
 *
 * @{
 */

/**
 * \file
 * sdcard.c
 *
 * \author W.Wallucks
 */

char cwdirectory[MAX_PATH+1] = "/";	//!< aktuelles Arbeitsverzeichnis
struct fat16_dir_struct* cwdir_ptr;	//!< zugehöriger Verzeichniseintrag

/**
 *	\ingroup sdcard
 *	Hardware für SD-Karte initialisieren
 */
void f16_init()
{
	spi_init();				// TWI-Bus
	configure_pin_cs();		// CS des Kartenslots
    sd_raw_init();			// Karte in SPI-Modus setzen
	cwdir_ptr = 0;			// noch kein directory gelesen
}

/**
 *	\ingroup sdcard
 *  Ein-/Ausstecken der Karte abhandeln
 */
void f16_check(void)
{
    if(sd_get_root_dir() && !sd_raw_available())
    {
        fat16_close_dir(cwdir_ptr);
        sd_close();
        cwdir_ptr = 0;
		#if USE_LOGDATEI
		if (logStatus.logfile) {
			f16_close(logStatus.logfile);
			logStatus.logfile = 0;
		}
		#endif

        SD_DEBUG("\r\nSD-Karte unplugged");
    }
    else if(!sd_get_root_dir() && sd_raw_available())
    {
        switch(sd_open())						// SD-Karte initialisieren
        {
            case SD_ERROR_NONE:
                cwdir_ptr = sd_get_root_dir();	// dir-Pointer setzen
				cwdirectory[0] = '/';			// start ist root
				cwdirectory[1] = 0;

				#if USE_LOGDATEI
				machineStatus.LogInit = 1;		// Logdatei in Mainloop initialisieren
				#endif

                SD_DEBUG("\r\nSD-Karte initialisiert");
				return;

            case SD_ERROR_INIT:
                SD_DEBUG("\r\nSD-Karte initialization failed");
                break;
            case SD_ERROR_PARTITION:
                SD_DEBUG("\r\nSD-Karte opening partition failed");
                break;
            case SD_ERROR_FS:
                SD_DEBUG("\r\nSD-Karte opening filesystem failed");
                break;
            case SD_ERROR_ROOTDIR:
                SD_DEBUG("\r\nSD-Karte opening root directory failed");
                break;
            default:
                SD_DEBUG("\r\nSD-Karte unknown error");
                break;
        }
    }
}

/**
 *	\ingroup sdcard
 *  Datei öffnen
 *
 * \see f16_close
 * \sa Anwendungsbeispiel log_status
 */
File* f16_open(const char *filename, const char *mode)
{
	if (!sd_get_fs())
		return 0;

    struct fat16_dir_entry_struct file_entry;

    while(fat16_read_dir(cwdir_ptr, &file_entry))
    {
        if(strcasecmp(file_entry.long_name, filename) == 0)
        {
            fat16_reset_dir(cwdir_ptr);

			File *newfile = fat16_open_file(sd_get_fs(), &file_entry);
			newfile->mode = *mode;
			if (*mode == 'a') {
				int32_t offset = 0;
				fat16_seek_file(newfile, &offset, FAT16_SEEK_END);
				SD_DEBUG("\r\nDatei %s geoeffnet %l Bytes",filename,offset);
			}
			return newfile;
        }
    }

	fat16_reset_dir(cwdir_ptr);

	// kein Dateieintrag gefunden -> bei mode == "a" oder "w" neuen erstellen
	if (*mode == 'a' || *mode == 'w') {
    	if(fat16_create_file(cwdir_ptr, filename, &file_entry)) {
			File *newfile = fat16_open_file(sd_get_fs(), &file_entry);
			newfile->mode = *mode;
			return newfile;
		}
	}

    return 0;
}

/**
 *	\ingroup sdcard
 *  Datei schließen
 *
 * \see f16_open
 * \sa Anwendungsbeispiel log_status
 */
void f16_close(File *stream)
{
	sd_raw_sync();
	fat16_close_file(stream);
}

/**
 *	\ingroup sdcard
 *  Datenpuffer auf Karte schreiben
 */
void f16_flush()
{
	sd_raw_sync();
}

/**
 *	\ingroup sdcard
 *  Datenzeiger in geöffneter Datei positionieren
 */
int f16_fseek(File *stream, int32_t offset, uint8_t origin)
{
	int32_t off = offset;
	return (int)fat16_seek_file(stream, &off, origin);
}

/**
 *	\ingroup sdcard
 *  ein Zeichen in Datei schreiben
 *
 * \see f16_puts
 * \see f16_puts_P
 */
int f16_fputc(char c, File *stream)
{
	return fat16_write_file(stream, (uint8_t *)&c, 1);
}

/**
 *	\ingroup sdcard
 *  string in Datei schreiben
 *
 * \see f16_puts_P
 */
int f16_fputs(char *s, File *stream)
{
	int i;

	for (i=0;;++i) {
		if (*s++ == 0)
			break;
	}

	return fat16_write_file(stream, (uint8_t *)s, i);
}

/**
 *	\ingroup sdcard
 *  string aus Flash-Memory in Datei schreiben
 *
 * \see f16_puts
 * \sa Anwendungsbeispiel log_status
  */
int f16_fputs_P(const char *s, File *stream)
{
	int i;
	char c;
	uint8_t *ptr = (uint8_t *)s;

	for (i=0;;++i) {		// Stringlänge ermitteln
		c = pgm_read_byte(ptr++);
		if (c == 0)
			break;
	}

	ptr = malloc(i);
	memcpy_P(ptr,s,i);

	i = fat16_write_file(stream, ptr, i);

	free(ptr);
	return i;
}


/**
 *	\ingroup sdcard
 *  einzelnes Zeichen aus Datei lesen
 *
 */
int f16_getc(File *stream)
{
	unsigned char c;
	int i;

	i = fat16_read_file(stream, &c, 1);	// gibt Anzahl gelesener Zeichen zurück oder 0/-1 bei EOF/Fehler
	if (i <= 0) {
		return -1;		// EOF
	}
	else {
		return (int)c;
	}
}

/**
 *	\ingroup sdcard
 *  eine Zeile aus Datei lesen
 *
 */
char *f16_gets(char *string, int size, File *stream)
{
	int i;

	i = fat16_read_file(stream, (uint8_t*)string, size-1);	// gibt Anzahl gelesener Zeichen zurück oder 0/-1 bei EOF/Fehler
	if (i <= 0) {
		return (char *)(0);		// EOF
	}
	else {
		char *ptr = string;
		while (*ptr != '\n' && --i) {
			++ptr;
		}
		--i;
		++ptr;
		*ptr = 0;	// terminieren


		if (i>0) {	// Datei wieder hinter CRLF positionieren
			int32_t off = -i;
			fat16_seek_file(stream, &off, FAT16_SEEK_CUR);
		}
		return string;
	}
}

/**
 *	\ingroup sdcard
 *  printf um in Datei zu schreiben
 *	\attention der Puffer für den formatierten String hat nur 64 char
 *
 * \sa Anwendungsbeispiel ::log_status
 */
int f16_printf_P(File *stream,const char *format,...)
{
	va_list ap;
		
	char outbuffer[64];
	uint8_t nchar = 0;

	va_start (ap, format);
	nchar = vsprintf_P(outbuffer, format, ap);
	fat16_write_file(stream, (unsigned char *)outbuffer, nchar);
	va_end(ap);
	return nchar;
}

/**
 *	\ingroup sdcard
 *  Datei auf Existenz prüfen
 */
uint8_t f16_exist(const char *filename)
{
	uint8_t iret = 0;

	if (!sd_get_fs())
		return 0;

    struct fat16_dir_entry_struct file_entry;

	SD_DEBUG("\r\nsdcard exist: file %s ", filename);
    if(sd_find_file_in_dir(cwdir_ptr, filename, &file_entry)) {
		iret = 1;
	}
	else
		SD_DEBUG("NOT ");

	SD_DEBUG("found.");

	return iret;
}

/**
 *	\ingroup sdcard
 *  Datei oder Verzeichniseintrag löschen
 */
uint8_t f16_delete(const char *filename)
{
	uint8_t iret = 0;
	if (!sd_get_fs())
		return iret;

    struct fat16_dir_entry_struct file_entry;

    if(sd_find_file_in_dir(cwdir_ptr, filename, &file_entry)) {
        if(fat16_delete_file(sd_get_fs(), &file_entry))
			iret = 1;
    }
	return iret;
}

/**
 *	\ingroup sdcard
 *  neues Verzeichnis anlegen
 */
uint8_t f16_mkdir(const char *dirname)
{
	uint8_t iret = 0;

	if (!sd_get_fs())
		return iret;

	struct fat16_dir_entry_struct dir_entry;

	iret = fat16_create_dir(cwdir_ptr, dirname, &dir_entry);

	return iret;
}

#endif	// USE_MMC
/**
 * @}
 */
