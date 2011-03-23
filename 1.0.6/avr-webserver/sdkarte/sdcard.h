
/**
 * \addtogroup sdcard SD/MMC Karte
 *
 * @{
  */

/**
 * \file
 * sdcard.h
 *
 * \author W.Wallucks
 */

/**
 * @}
 */
#ifndef SDCARD_H
#define SDCARD_H

/*------------------------------------
*	globale Variable
*/
extern char cwdirectory[MAX_PATH+1];		// aktuelles Arbeitsverzeichnis
extern struct fat16_dir_struct* cwdir_ptr;	// zugehöriger Verzeichniseintrag


/*------------------------------------
*	typedefs
*/

/*------------------------------------
*	Prototypen
*/
void 	f16_init(void);
void 	f16_check(void);
File* 	f16_open(const char *filename, const char *mode);
void	f16_close(File *stream);
void	f16_flush(void);
int 	f16_fseek(File *stream, int32_t offset, uint8_t whence);
int 	f16_fputc(char c, File *stream);
int 	f16_fputs(char *s, File *stream);
int 	f16_fputs_P(const char *s, File *stream);
int 	f16_getc(File *stream);
char 	*f16_gets(char *, int, File *);
int 	f16_printf_P(File *fp, const char *format,...);
uint8_t f16_exist(const char *);
uint8_t f16_delete(const char *);
uint8_t f16_mkdir(const char *);

#endif
