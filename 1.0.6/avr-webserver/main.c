/*----------------------------------------------------------------------------
 Copyright:      Radig Ulrich  mailto: mail@ulrichradig.de
 Author:         Radig Ulrich & W.Wallucks
 Remarks:        
 known Problems: none
 Version:        24.10.2007
 Description:    Webserver uvm.

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
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "config.h"

#include "usart.h"
#include "networkcard/enc28j60.h"
#include "stack.h"
#include "timer.h"
#include "base64.h"
#include "httpd.h"
#include "telnetd.h"

#if USE_WOL
#include "wol.h"
#endif

#if USE_NTP
  #include "ntp.h"
#endif

#if USE_SER_LCD
  #include "lcd.h"
  #include "udp_lcd.h"
#endif

#if USE_ADC
  #include "analog.h"
#endif

#if USE_CAM
  #include "camera/cam.h"
  #include "camera/servo.h"
#endif

#if USE_MAIL
  #include "sendmail.h"
#endif

#if USE_DNS
  #include "dns.h"
#endif

#if USE_MMC
  #include "sdkarte/fat16.h"
  #include "sdkarte/sdcard.h"
#endif

#if TCP_SERVICE
  #include "tcpservice/tcpcmd.h"
  #include "tcpservice/tcpsrv.h"
#else
  #include "cmd.h"
#endif

#if USE_RC5
	#include "rc5/rc5.h"
#endif

#include "messung.h"

/**
 * \mainpage Webmodule
 *
 * In diesem Projekt werden einige \ref mainloop "Erweiterungen zur ETH32-Software" von Ulrich Radig beschrieben.
 * Als Hardware wird das Webmodul von Ulrich Radig (http://www.ulrichradig.de/home/index.php/avr/avr-webmodule)
 * verwendet. Die ursprüngliche Software findet sich hier http://www.ulrichradig.de/home/index.php/avr/eth_m32_ex
 *
 * \image html Webmodule.jpg "Ulrich Radigs Webmodul mit SD-Karte"
 *
 *	Des weiteren wurde das FAT16-Filesystem von Roland Riegel verwendet. 
 *	(http://www.roland-riegel.de/sd-reader/index.html)<br>
 * sowie Code von Martin Thomas (1-Wire DS18B20) und Peter Dannegger (1-Wire und RC5)
 */

/**
 * \page hardware	Hardwareanpassung
 *
 * @{
 *	Der SD-Kartenslot aus Ulis Shop hat einen Schalter, der geschlossen ist wenn 
 *	die Karte gesteckt ist. Leider ist der Schalter aber auf der Originalplatine nicht 
 *	verdrahtet.
 *
 *	\b Abhilfe: einen der Anschlusspins vom Slot (befinden sich an der Ecke 
 *	wo der Bestückungsaufdruck MMC1 ist) auf Masse festlöten und den anderen an PA6 
 *	mit einem kleinen Draht verbinden. \n
 *	Ohne den Schalter gibt es einen langen Timeout, wenn man auf die SD-Karte
 *	zugreifen will und es ist keine Karte gesteckt. Ebenso ist es durch den Schalter
 *	möglich einen Wechsel der Karte zu erkennen und sie neu zu initialisieren.
 *	Siehe dazu die Funktion \ref f16_check().
 *
 *	Ohne diese Hardwareänderung muss im Quelltext in <em>sd_raw_config.h</em> die Zeile
 *	\code #define get_pin_available() ((PINA & (1<<PA6))) \endcode
 *	nach
 *	\code #define get_pin_available() (1) \endcode
 *	abgeändert werden.
 *
 *	Grundsätzlich hier noch ein paar Anmerkungen:
 *
 *	\li <b>Die Webseiten welche von der SD-Karte gelesen werden sollen müssen sich alle im
 *	Hauptverzeichnis der Karte befinden.</b> Der Code um Dateien aus verschiedenen
 *	Unterverzeichnissen zusammenzusuchen war mir bisher zu aufwendig und ist für
 *	einen Miniwebserver wohl auch nicht nötig.
 *
 *	\li Die <b>Anzeige der Dateien mit Datum, Größe, Berechtigungen etc.</b> ist in den RFC
 *	für FTP nicht definiert. Ich habe bisher keine Ahnung, wie eine allgemeingültige
 *	Verzeichnisanzeige auszusehen hat. Je nach FTP-Client wird daher die
 *	Verzeichnisstruktur unterschiedlich dargestellt. Mit FileZilla funktioniert es
 *	bei mir jedenfalls recht gut.
 *
 *	\li <b>Wie muss man nun vorgehen, wenn man von der SD-Karte eine HTML-Seite laden möchte?</b> \n
 *	html-Datei erstellen und im Root-Verzeichnis der Karte ablegen. Zusätzliche Dateien
 *	(Bilder) auch ins Root-Verzeichnis ablegen, da der Code um die Dateien im
 *	Unterverzeichnis zu suchen zuviel Overhead mit sich bringt. Falls man index.htm auf der
 *	Karte abspeichert wird sie direkt aufgerufen. Ansonsten die Datei im Browser
 *	angeben <tt>http://192.168.0.99/default.htm</tt> falls man default.htm abspeichert.
 *
 *	\li <b>Wie stellt man das an, dass Dateien auf der Karte über das Internet abrufbar sind?</b> \n
 *	Mit FileZilla drauf zugreifen! Wenn man von "außen" auf sein Webmodul zugreifen will,
 *	muss man Dyndns einrichten. In der Fritzbox gibt man dann unter Portfreigabe das
 *	Webmodul nach außen frei. Am Einfachsten setzt man hierzu den Eintrag "Exposed Host"
 *	auf die Adresse seines Webmoduls. Es sollte kein Problem bereiten das Teil komplett
 *	freizugeben. 
 *
 *	\li <b>Kann ich auch vom AVR-Server darauf zugreifen und wie muss man vorgehen um
 *	Dateien zu erzeugen zu löschen zu laden usw. ?</b>
 *	Das Webmodul ist ein FTP-Server. Eine FTP-Client-Funktion ist nicht dabei!
 *	Theoretisch geht das schon wenn man es programmiert.
 *
 *	Praktisch die gesamte Grundkonfiguration des Moduls erfogt in der Datei config.h. Dort
 *	sind je nach Hardwareausstattung und benötigter Funktionalität einige <tt>define</tt>
 *	vorgegeben, die durch setzen von null(0) oder eins(1) entsprechend aus- oder angeschaltet
 *	werden können.
 *
 * @}
 *
 */

/**
 * \defgroup main	Hauptprogramm
 *
 * siehe auch Beschreibung der \ref mainloop "Hauptschleife" weiter unten.
 * @{
 */

/**
 * \file Hauptprogramm
 *
 * \author Ulrich Radig & W.Wallucks
 *
 */

/*
**	globale Variable
*/
volatile ANLAGEN_STATUS anlagenStatus;		//!< aktueller Zustand der Schalter und Zähler
volatile PROZESS_STATUS machineStatus;		//!< Prozessstatus und aktuelle Werte
#if USE_MMC
		 LOG_STATUS		logStatus;			//!< Status der Logdatei
#endif
volatile SOLL_STATUS 	anlagenSollStatus;	//!< Zustandsbeschreibung der angeschlossenen Schalter und Relais

//----------------------------------------------------------------------------
/**
 *	Variablenarry zum Einfügen in Webseite \%VA\@00 bis \%VA\@09<br>
 *	in den ersten Speicherplätzen werden bei aktiviertem ADC die
 *	gemessenen Analogwerte gespeichert
 */
unsigned int var_array[MAX_VAR_ARRAY] = {10,50,30,0,0,0,0,0,0,0};


FILE usart_out = FDEV_SETUP_STREAM(usart_putchar, NULL, _FDEV_SETUP_WRITE);

/**
 **	lokale Prototypen für Timerfunktionen
*/
void read_T(void);

//----------------------------------------------------------------------------
//
int main(void)
{
	//Konfiguration der Ausgänge bzw. Eingänge
	//definition erfolgt in der config.h
	DDRA = OUTA;
	DDRC = OUTC;
	DDRD = OUTD;
	
    unsigned long a;
	#if USE_SERVO
		servo_init ();
	#endif //USE_SERVO
	
    usart_init(BAUDRATE); 	// setup the USART
	stdout = &usart_out;	// set standard lib-functions
	
	#if USE_ADC
		ADC_Init();
	#endif
	
	printf_P(PSTR("\n\rSystem Ready\n\r"));
    printf_P(PSTR("Compiliert am "__DATE__" um "__TIME__"\r\n"));
    printf_P(PSTR("Compiliert mit GCC Version "__VERSION__"\r\n"));
	for(a=0;a<1000000;a++){asm("nop");};

	//Applikationen starten
	timer_init();	// Timer starten - von stack_init() hierher verschoben
	stack_init();
	httpd_init();
	telnetd_init();
	
	//Spielerrei mit einem LCD
	#if USE_SER_LCD
	//udp_lcd_init();
	lcd_init();
	lcd_clear();
	back_light = 10;
	lcd_print(0,0,"System Ready");
	#endif
	//Ethernetcard Interrupt enable
	ETH_INT_ENABLE;
	
	//Globale Interrupts einschalten
	sei(); 
	
	#if USE_CAM
		#if USE_SER_LCD
		lcd_print(1,0,"CAMERA INIT");
		#endif //USE_SER_LCD
	for(a=0;a<2000000;a++){asm("nop");};
	cam_init();
	max_bytes = cam_picture_store(CAM_RESOLUTION);
		#if USE_SER_LCD
		back_light = 10;
		lcd_print(1,0,"CAMERA READY");
		#endif //USE_SER_LCD
	#endif //USE_CAM
	
	#if USE_DNS
		dns_init();
	#endif

	#if USE_NTP
        ntp_init();
	#endif //USE_NTP
	
	#if USE_WOL
        wol_init();
	#endif //USE_WOL
    
    #if USE_MAIL
        sendmail_init();
	#endif //USE_MAIL
    
	#if TCP_SERVICE
		tcpsrvd_init();
	#endif

    #if USE_MMC
		f16_init();
	#endif

	#if USE_RC5
	rc5_init();
	#endif

	//**************************************************
	#define logdata usart_write
	messung_init();
	
	 /**
	 * \ingroup main
	 * \anchor mainloop
 	 * \b Hauptschleife
	 *
	 *	Die Hauptschleife wird über mehrere Flags gesteuert.
	 *  Diese Flags sind in der Struktur #PROZESS_STATUS <tt>machineStatus</tt>
	 *	zusammengefasst.
	 *	Das Setzen der Flags geschieht gewöhnlich in Interrupts (Timer, PIN changed etc.)
	 *	oder durch Über-/Unterschreiten von Schwellwerten.
	 *
	 *	Als erstes wird in der Hauptschleife die Zeit hochgezählt. Der Timerinterrupt
	 *	zählt nur das Flag #PROZESS_STATUS.timeChanged. Dadurch ist es möglich bei Erreichen
	 *  einer vorgegebenen Schaltzeit auch längere Aktionen auszuführen ohne auf Dauer eine
	 *	Gangungenauigkeit zu riskieren. Die zwischenzeitlich vergangenen Sekunden werden
	 *	notfalls einzeln in den nächten Schleifendurchgängen beschleunigt nachgeholt.
	 *
	 *	Danach werden gesetzte Flags einzeln abgefragt, eine entsprechende Funktion zum
	 *	Ausführen der Aktion aufgerufen und das Flag wieder zurückgesetzt.
	 *
	 *	Regelmäßig wird dabei 
	 *	- die Ethernetschnittstelle mit #eth_get_data() gepollt;
	 *	- die serielle Schnittstelle auf Ende des
	 *	  Befehls abgefragt #usart_status.usart_ready
	 *	- bei TELNET-Verbindung auf Port 23 werden Zeichen
	 *	  von USART->TCP geschickt <tt>telnetd_send_data();</tt>
	 *
	 */	
	while(1)
	{
		if (machineStatus.timeChanged) {
			// eine Sekunde hochzählen und
			// auf Änderungen zu vordefinierten Zeiten reagieren
			TM_AddOneSecond();
			machineStatus.timeChanged--;
			eth.timer = 1;

			// Countdown-Timer bei Bedarf runterzählen
			if (machineStatus.Timer1) {
				machineStatus.Timer1--;
				if (!machineStatus.Timer1) {	// Zero?
					// bei Zero (0) Flag für Aktion setzen
					// oder sofort ausführen
					PORTA &= ~(1<<2);	// Pin 2 resetten
				}
			}

			// ... und der nächste Countdown
			if (machineStatus.Timer2) {
				machineStatus.Timer2--;
				if (!machineStatus.Timer2) {			// Zero?
					if (machineStatus.Timer2_func)
						machineStatus.Timer2_func();	// ausführen
				}
			}

			#if USE_SER_LCD
			if (back_light) {
				if (--back_light == 0) {
					lcd_clear();				// Licht aus !
				}
			}
			#endif

		}

		/**
		*	\anchor t3timer
		*	<H3>Countdown-Timer</H3>
		*
		*	Timer3 wird in ISR(TIMER1_COMPA_vect) im Takt von TIMERBASE
		*	heruntergezählt. Bei Erreichen von Null wird das Flag Time3Elapsed
		*	gesetzt, was hier ausgewertet wird. Falls der Funktionszeiger
		*	Timer3_func nicht NULL ist, wird die entsprechende Funktion
		*	aufgerufen.
		*
		*	Beispiel:
		*	\code
		*	if (!machineStatus.Timer3) {			// wenn Timer3 nicht belegt ist ...
		*		machineStatus.Timer3 = start_OW();	// liefert Wartezeit (750 ms) zurück und startet Timer3
		*		machineStatus.Timer3_func = read_T;	// Funktionspointer für abgelaufene Zeit setzen
		*	}
		*	\endcode
		*
		*/
		if (machineStatus.Time3Elapsed) {
			machineStatus.Time3Elapsed = false;
			if (machineStatus.Timer3_func)
				machineStatus.Timer3_func();			// ausführen
		}


		#if USE_MMC
		f16_check();	// Ein-/Ausstecken der SD-Karte erkennen
		#endif

		#if USE_SCHEDULER

		#if USE_LOGDATEI
		if (machineStatus.LogInit ) {
			// neue Logdatei anlegen
			log_init();
			machineStatus.initialisieren = true;
			machineStatus.LogInit = false;
		}
		#endif

		if (machineStatus.initialisieren) {
			// Schaltzeiten neu einlesen
			initSchaltzeiten(0);
			machineStatus.initialisieren = 0;
		}

		if (machineStatus.regeln) {
			// Anlage entsprechend regeln
			// alle 10 Minuten wegen Schaltzuständen
			SOLL_STATUS aktSoll;
			TM_SollzustandGetAktuell(&aktSoll);
			regelAnlage(&aktSoll);
			machineStatus.regeln = 0;
		}
		#endif

		#if USE_OW
		if (machineStatus.Tlesen) {
			// Temperatursensoren lesen
			machineStatus.Tlesen = 0;
			machineStatus.Timer3 = start_OW();	// liefert Wartezeit (750 ms) zurück
			machineStatus.Timer3_func = read_T;	// Funktionspointer für abgelaufene Zeit setzen
		}
		#endif

		#if USE_LOGDATEI
		if (machineStatus.LogSchreiben) {
			// schreiben
			log_status();
			machineStatus.LogSchreiben = false;
		}
		#endif

		/**
		*	<H3>Beispiele zur Steuerung über externe Eingänge</H3>
		*
		*	<tt>machineStatus.PINCchanged</tt> wird per Interrupt gesetzt,
		*	wenn eine Änderung an einem der als Eingang geschalteten Pins an Port-C
		*	eintritt.
		* \see ISR(PCINT2_vect)
		*
		*	Angegeben sind hier vier Beispiele
		*	- ein einfacher Zähler, der die Änderungen an PINC7 zählt
		*	- das Messen der Zeit zwischen Ein- und Ausschalten (Betriebsstundenzähler)
		*	- Zählen von Pulsen, solange ein anderer Eingang eingeschaltet ist
		*	- Auslösen einer definierten Aktion, wie zum Beispiel senden einer E-Mail.
		*	  Das kann eingesetzt werden um eine Benachrichtigung zu schicken, falls ein
		*	  Schalter angesprochen hat.
		*
		*	Initialisiert wird Port C in <tt> \link messen messung_init()\endlink </tt> und für das Setzen
		*	der Statusänderung ist der Interrupt <tt>ISR(PCINT2_vect)</tt> zuständig.
		*	Beide Funktionen befinden sich in messung.c
		*/
		if (machineStatus.PINCchanged != 0) {

			// Eingang hat sich geändert

			if (machineStatus.PINCchanged & 1<<SENS_PIN1) {
				/**
				 * - 1. Beispiel
				 * Zustand von Sensor1 hat sich geändert<br>
				 * \b Beispielaktion:<br>
				 * zählen wie oft der PIN eingeschaltet wurde<br>
				 * der Zählerstand von PINCcounter kann dann<br>
				 * regelmässig ins Logfile geschrieben werden<br>
				*/

				if ( !(machineStatus.PINCStatus & 1<<SENS_PIN1) ) {	// falls eingeschaltet
					anlagenStatus.Zaehler1++;
				}
			}

			if (machineStatus.PINCchanged & 1<<SENS_PIN2) {
				/**
				 * - 2. Beispiel
				 * Zustand von Sensor2 hat sich geändert<br>
				 * \b Beispielaktion:<br>
				 * Zeitspanne zwischen Ein- und Ausschalten messen<br>
				 * und in Logdatei festhalten
				*/
				if ( !(machineStatus.PINCStatus & 1<<SENS_PIN2) ) {	// falls eingeschaltet
					anlagenStatus.Zaehler2 = time;
				} 
				else {
					logdata("Sensor 2 Zeit: %i Sekunden",time - anlagenStatus.Zaehler2);
					anlagenStatus.Zaehler2=0;
				}
			}

			if (machineStatus.PINCchanged & 1<<SENS_PIN3) {
				/**
				 * - 3. Beispiel
				 * Zustand von Sensor3 hat sich geändert<br>
				 * <b>Beispielaktion:</b>
				 * in Abhängigkeit von Sensor2 Pulse an Sensor3 zählen
				 */
				if ( !(machineStatus.PINCStatus & 1<<SENS_PIN2) ) {
					// nur zählen, wenn Sensor2 eingeschaltet
				   	anlagenStatus.Zaehler3++;
				} 
			}

			if (machineStatus.PINCchanged & 1<<SENS_PIN4) {
				/**
				 * - 4. Beispiel
				 * Zustand von Sensor4 hat sich geändert<br>
				 * \b Beispielaktion:<br>
				 * vordefinierte E-Mail schicken
				 */
				if ( !(machineStatus.PINCStatus & 1<<SENS_PIN4) ) {
					#if USE_MAIL
			    	sendmail(1);
					#else
					usart_write("\r\nEine E-Mail sollte gesendet werden.");
					#endif
				}
			}

			machineStatus.PINCchanged = 0;
		}

		// free running ADC
		#if USE_ADC
		ANALOG_ON;
		#endif
	    eth_get_data();
		
        //Terminalcommandos auswerten
		if (usart_status.usart_ready){
            usart_write("\r\n");
			if(extract_cmd(&usart_rx_buffer[0]))
			{
			#if USE_MMC
				usart_write("\r\nSD:%s>",cwdirectory);
			#else
				usart_write("Ready\r\n\r\n");
			#endif
			}
			else
			{
				usart_write("ERROR\r\n\r\n");
			}
			PORTD ^= (1<<PD6);
			usart_status.usart_ready =0;
		}
		
		/**
		*	\anchor RC5Beispiel
		*	<H3>Beispiel zur Steuerung mit IR-Fernbedienung (RC5 Code)</H3>
		*
		*	Per Fernbedienung lassen sich hier die Temperaturen der 1-Wire
		*	Sensoren auf dem LCD-Display darstellen. Dazu müssen 3 Tasten
		*	in rc5.h mit '#define' definiert werden. (Der gesendete  Code ist
		*	eventuell bei den Fernbedienungen unterschiedlich).
		*
		*	Ebenso sind in RC5_TABLE[] einige Funktionen aus der tcpcmd
		*	eingetragen, die sich über einzelne Tasten der Fernbedienung
		*	direkt aufrufen lassen.
		*
		*	Um die gesendeten Codes <tt>seiner</tt> Fernbedienung zu bestimmen
		*	kann man die Auskommentierung der "usart_write"-Befehle aufheben.
		*	Die empfangenen Codes werden dann auf der seriellen Schnittstelle
		*	ausgegeben.
		*/
		#if USE_RC5
	    cli();
    	int i = rc5_data;			// read two bytes from interrupt !
    	rc5_data = 0;
    	sei();
		if( i ){
			if ( i != machineStatus.lastRC5data ) {
				// neuer Tastendruck
				machineStatus.lastRC5data = i;	// letzten Tastendruck speichern

				uint8_t device = (i >> 6 & 0x1F);
				uint8_t keycode = (i & 0x3F) | (~i >> 7 & 0x40);

				//usart_write("\r\n%i",( i >> 11 & 1));	// Toggle Bit
				//usart_write(" %i", device);			// Device address
				//usart_write(" %i -",keycode); 		// Key Code

				if ( keycode == RC5KEY_OW ) {			// Temperatur neu lesen
					lese_Temperatur();
					machineStatus.disp_index = 0;
					#if USE_SER_LCD
					lcd_clear();
					#endif
				}
				else if ( keycode == RC5KEY_UP ) {
					if ( --(machineStatus.disp_index) < 0 )
						machineStatus.disp_index = MAXSENSORS - 1;
				}
				else if ( keycode == RC5KEY_DWN ) {
					if ( ++(machineStatus.disp_index) >= MAXSENSORS )
						machineStatus.disp_index = 0;
				}

				if ( keycode == RC5KEY_OW || keycode == RC5KEY_UP
										  || keycode == RC5KEY_DWN) {
					#if USE_SER_LCD
					char zeile[20];

					back_light = 30;	// für 30 Sekunden einschalten
					lcd_print(0,0,"Temp. %2i: ",(machineStatus.disp_index+1));
					dtostrf(ow_array[machineStatus.disp_index] / 10.0,5,1,zeile);
					lcd_print_str(zeile);
					lcd_print_str("C");
					#endif
				}
				else if ( keycode < RC5maxcmd ) {
					argv = 0;	// keine Parameter
					((cmd_fp)pgm_read_word(&RC5_TABLE[keycode]))(0);
				}
				else {
					printf_P(PSTR("\r\nunbekannter RC5 Code: %i / %i\r\n"),device, keycode);
				}
			}
			else {
				// Taste wird gehalten -> nur Volume, Channel etc. auswerten
				//usart_putchar(( i >> 11 & 1) + '0'), 0);	// Toggle Bit
			}
		}
		#endif

        //Wetterdaten empfangen (Testphase)
        #if GET_WEATHER
        http_request ();
        #endif

        //Empfang von Zeitinformationen
		#if USE_NTP
		if(!ntp_timer){
			ntp_timer = NTP_REFRESH;
			ntp_request();
		}
		#endif //USE_NTP
		
        //Versand von E-Mails
        #if USE_MAIL
        if (machineStatus.sendmail != 0)
        {
            machineStatus.sendmail = sendmail(machineStatus.sendmail);
        }
        #endif

        //Rechner im Netzwerk aufwecken
        #if USE_WOL
        if (wol_enable == 1)
        {
            wol_enable = 0;
            wol_request();
        }
        #endif //USE_WOL
           
		//USART Daten für Telnetanwendung?
		telnetd_send_data();
    }
	return(0);
}


#if USE_OW || DOXYGEN
/**
 *	\ingroup onewire
 *	Diese Funktion wird nach Ablauf des Messintervalls der 1-Wire Sensoren
 *	durch einen Funktionszeiger aufgerufen.
 */
void read_T(void)
{
	lese_Temperatur();
	machineStatus.LogSchreiben = true;		// danach gleich Logdatei schreiben
	machineStatus.Timer3_func = NULL;		// vorsichtshalber Funktion löschen.

	#if USE_SER_LCD
	char zeile[20];

	lcd_clear();
	back_light = 10;	// 10 Sekunden einschalten
	lcd_print(0,0,"Aussen ");
	dtostrf(ow_array[0] / 10.0,5,1,zeile);
	lcd_print_str(zeile);
	lcd_print_str("C");
	lcd_print(1,0,"Innen  ");
	dtostrf(ow_array[1] / 10.0,5,1,zeile);
	lcd_print_str(zeile);
	lcd_print_str("C");
	#endif
}
#endif

/**
 * @}
 */
