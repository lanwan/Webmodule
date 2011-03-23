
/*
 * Copyright (c) 2006-2008 by Roland Riegel <feedback@roland-riegel.de>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#ifndef SD_RAW_CONFIG_H
#define SD_RAW_CONFIG_H

/**
 * \addtogroup sd
 *
 * @{
 */
/**
 * \addtogroup sd_raw
 *
 * @{
 */
/**
 * \file
 * MMC/SD support configuration (license: GPLv2 or LGPLv2.1)
 */

/**
 * \ingroup sd_raw_config
 * Controls MMC/SD write support.
 *
 * Set to 1 to enable MMC/SD write support, set to 0 to disable it.
 */
#define SD_RAW_WRITE_SUPPORT 1

/**
 * \ingroup sd_raw_config
 * Controls MMC/SD write buffering.
 *
 * Set to 1 to buffer write accesses, set to 0 to disable it.
 *
 * \note This option has no effect when SD_RAW_WRITE_SUPPORT is 0.
 */
#define SD_RAW_WRITE_BUFFERING 1

/**
 * \ingroup sd_raw_config
 * Controls MMC/SD access buffering.
 * 
 * Set to 1 to save static RAM, but be aware that you will
 * lose performance.
 *
 * \note When SD_RAW_WRITE_SUPPORT is 1, SD_RAW_SAVE_RAM will
 *       be reset to 0.
 */
#define SD_RAW_SAVE_RAM 0

/* defines for customisation of sd/mmc port access */

/*
 * - PortB Pin4 Slave Select für SD-Karte (not used!)
 */
//#define configure_pin_ss() DDRB |= (1 << PB4)

/**
 * \port
 * - PortB Pin1 Chip Select für SD-Karte
 */
#define configure_pin_cs() DDRB |= (1 << PB1)

/**
 * \port
 * - PortA Pin6 auf Eingang und PullUp einschalten;
 * Schalter zieht auf Masse bei gesteckter SD-Karte
 */
#define configure_pin_available() DDRA &= ~(1 << PA6); PORTA |= (1<<PA6)

/**
 * \port
 * - PortA Pin7 auf Eingang und PullUp einschalten;
 * Schalter zieht auf Masse bei Schreibschutz für SD-Karte
 */
#define configure_pin_locked() DDRA &= ~(1 << PA7); PORTA |= (1<<PA7)

// PIN6 == 1 -> keine Karte; 0 -> Karte drin
#define get_pin_available() ((PINA & (1<<PA6)))
#define get_pin_locked() 0	//((PINA & (1<<PA7)))

#define select_card() PORTB &= ~(1 << PB1)
#define unselect_card() PORTB |= (1 << PB1)


/**
 * @}
 * @}
 */

/* configuration checks */
#if SD_RAW_WRITE_SUPPORT
#undef SD_RAW_SAVE_RAM
#define SD_RAW_SAVE_RAM 0
#else
#undef SD_RAW_WRITE_BUFFERING
#define SD_RAW_WRITE_BUFFERING 0
#endif

#endif

