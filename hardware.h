/*
 *  hardware.h
 *  MRF49XA
 *
 *  Created by William Dillon on 11/1/10.
 *  Copyright 2010 Oregon State University. All rights reserved.
 *
 */

#ifndef HARDWARE_H
#define HARDWARE_H

#define F_CPU 8000000UL	// 8 Mhz internal clock

#include <avr/io.h>
#include <avr/interrupt.h>

/* These defines are included for the SPI library
 *
 */ 
#define SOFTWARE_SPI

#define SPI_MISO_PORTx	PORTB
#define SPI_MISO_PINx	PINB
#define SPI_MISO_DDRx	DDRB
#define SPI_MISO_BIT	3

#define SPI_MOSI_PORTx	PORTB
#define SPI_MOSI_PINx	PINB
#define SPI_MOSI_DDRx	DDRB
#define SPI_MOSI_BIT	2

#define SPI_SCLK_PORTx	PORTB
#define SPI_SCLK_PINx	PINB
#define SPI_SCLK_DDRx	DDRB
#define SPI_SCLK_BIT	1

/* These defines set the chip select line and interrupt for the MRF module
 *
 */
#define MRF_CS_PINx		PINB
#define MRF_CS_PORTx	PORTB
#define MRF_CS_DDRx		DDRB
#define MRF_CS_BIT		4

#define MRF_FSEL_PORTx	PORTB
#define MRF_FSEL_DDRx	DDRB
#define MRF_FSEL_BIT	5

#define MRF_DIO_PINx	PINB
#define MRF_DIO_PORTx	PORTB
#define MRF_DIO_DDRx	DDRB
#define MRF_DIO_BIT     6

#define MRF_RCLK_PINx	PINB
#define MRF_RCLK_PORTx	PORTB
#define MRF_RCLK_DDRx	DDRB
#define MRF_RCLK_BIT	7

#define LED_PORTx       PORTC
#define LED_DDRx        DDRC
#define LED_POWER       4
#define LED_TX          5
#define LED_RX          6

#define MRF_IRO_PINx	PINC
#define MRF_IRO_PORTx	PORTC
#define MRF_IRO_DDRx	DDRC
#define MRF_IRO_BIT		7

#define MRF_IRO_VECTOR	INT4_vect

// These are macros for setting up the interrupts for the MRF
#define MRF_INT_SETUP()	EICRB |= (1 << ISC41)
#define MRF_INT_MASK()	EIMSK |= (1 << INT4)

/*******************************************************************************
 * These defines set either the soldered-on characteristics of the MRF module,
 * or they are specific to this application. This includes the frequency band,
 * clock output, and pin settings.
 *
 ******************************************************************************/
// Set the module to 434 Mhz band, with 10pF series capactance crystal
#define MRF_GENCREG_SET		(MRF_GENCREG | (MRF_LCS & MRF_LCS_MASK) | MRF_FBS_434)
#define MRF_AFCCREG_SET		(MRF_AFCCREG | MRF_AUTOMS_INDP | MRF_ARFO_3to4 | MRF_HAM | MRF_FOFEN) // defaults
#define MRF_PLLCREG_SET		(MRF_PLLCREG | MRF_CBTC_5p)
#define MRF_FIFOSTREG_SET	(MRF_FIFORSTREG | MRF_DRSTM | ((8 << 4) & MRF_FFBC_MASK))

#endif