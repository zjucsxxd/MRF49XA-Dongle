/*
 *  spi.c
 *  MRF49XA
 *
 *  Created by William Dillon on 11/1/10.
 *  Copyright 2010 Oregon State University. All rights reserved.
 *
 */

#include "spi.h"

// This variable is vital for SW SPI, and 16 bit HW SPI.
volatile uint16_t spi_read_buffer;

volatile uint8_t mutex;

// This function assumes that the CS pins for peripherals are already "inactive"
void spi_init(void)
{
#ifndef SOFTWARE_SPI
	// Setup the hardware SPI interface
	SPCR   =  0x50;		// set up SPI mode
	SPSR   =  0x01;		// double speed operation	
#endif

	// Setup the port pins for HW and SW SPI interface
	SPI_MISO_PORTx &= ~(1 << SPI_MISO_BIT); // Set port value to 0 (pullups off)
	SPI_MISO_DDRx  &= ~(1 << SPI_MISO_BIT); // Enable input on MISO
	SPI_MOSI_PORTx &= ~(1 << SPI_MOSI_BIT); // Set port value to 0
	SPI_MOSI_DDRx  |=  (1 << SPI_MOSI_BIT); // Enable output on MOSI
	SPI_SCLK_PORTx &= ~(1 << SPI_SCLK_BIT); // Set port value to 0
	SPI_SCLK_DDRx  |=  (1 << SPI_SCLK_BIT); // Enable output on SCLK
}

// In HW SPI mode, we still write to spi_read_buffer to enable 16 bit reads
uint8_t spi_read8()
{
	uint8_t	temp = spi_read_buffer;
	spi_read_buffer = 0x0000;
	return temp;
}

uint16_t spi_read16()
{
	return spi_read_buffer;
}

// To perform the SPI write functions, we have to do more work in the SW mode.
#ifdef SOFTWARE_SPI
void spi_write8(uint8_t data, volatile uint8_t *enable_port, uint8_t enable_pin)
{
	int i = 0;

	// Spin on the mutex
	while (mutex);
	mutex = 1;
	
	// Bring the enable pin low to enable SPI on the peripheral
	*enable_port &= ~(1 << enable_pin);
	spi_read_buffer = 0;
	
	for (i = 0; i < 8; i++) {
		if (data & 0x80) {
			SPI_MOSI_PORTx |=  (1 << SPI_MOSI_BIT); // Set port value to 1
		} else {
			SPI_MOSI_PORTx &= ~(1 << SPI_MOSI_BIT); // Set port value to 0
		}
		
		// Shift read buffer contents
		spi_read_buffer = spi_read_buffer << 1;
		
		// Data valid on rising edge of the clock
		SPI_SCLK_PORTx |=  (1 << SPI_SCLK_BIT); // Set clock value to 1
		
		// Read MISO
		if (bit_is_set(SPI_MISO_PINx, SPI_MISO_BIT)) {
			spi_read_buffer |= 0x01;
		}
		
		// Shift output data
		data = data << 1;
		
		// Data change on falling edge
		SPI_SCLK_PORTx &= ~(1 << SPI_SCLK_BIT); // Set clock value to 0		
	}
	
	// Return the enable pin and MOSI to the inactive state
	SPI_MOSI_PORTx &= ~(1 << SPI_MOSI_BIT); // Set port value to 0
	*enable_port |= (1 << enable_pin);
	
	mutex = 0;
}

void spi_write16(uint16_t data, volatile uint8_t *enable_port, uint8_t enable_pin)
{
	int i = 0;
	
	// Spin on the mutex
	while (mutex);
	mutex = 1;
	
	// Bring the enable pin low to enable SPI on the peripheral
	*enable_port &= ~(1 << enable_pin);
	spi_read_buffer = 0;
	
	for (i = 0; i < 16; i++) {
		if (data & 0x8000) {
			SPI_MOSI_PORTx |=  (1 << SPI_MOSI_BIT); // Set port value to 1
		} else {
			SPI_MOSI_PORTx &= ~(1 << SPI_MOSI_BIT); // Set port value to 0
		}

		// Shift read buffer contents
		spi_read_buffer = spi_read_buffer << 1;
		
		// Read MISO
		if (bit_is_set(SPI_MISO_PINx, SPI_MISO_BIT)) {
			spi_read_buffer |= 0x01;
		}
		
		// Data valid on rising edge of the clock
		SPI_SCLK_PORTx |=  (1 << SPI_SCLK_BIT); // Set clock value to 1
		
		// Shift input data
		data = data << 1;
		
		// Data change on falling edge
		SPI_SCLK_PORTx &= ~(1 << SPI_SCLK_BIT); // Set clock value to 0		
	}
	
	// Return the enable pin and MOSI to the inactive state
	SPI_MOSI_PORTx &= ~(1 << SPI_MOSI_BIT); // Set port value to 0
	*enable_port |= (1 << enable_pin);
	
	mutex = 0;
}

#else

void spi_write8(uint8_t data, volatile uint8_t *enable_port, uint8_t enable_pin)
{
	// Spin on the mutex
	while (mutex);
	mutex = 1;
	
	// Bring the enable pin low to enable SPI on the peripheral
	*enable_port &= ~(1 << enable_pin);
	
	SPDR = data;
	
	loop_until_bit_is_set(SPSR, SPIF);
	
	spi_read_buffer = (uint16_t)SPDR;
	
	// Return the enable pin to the inactive state
	*enable_port |= (1 << enable_pin);
	
	mutex = 0;
}

void spi_write16(uint16_t data, volatile uint8_t *enable_port, uint8_t enable_pin)
{
	// Spin on the mutex
	while (mutex);
	mutex = 1;
	
	// Bring the enable pin low to enable SPI on the peripheral
	*enable_port &= ~(1 << enable_pin);
	
	// Byte 1
	SPDR = (uint8_t)((data & 0xFF00) >> 8);
	
	loop_until_bit_is_set(SPSR, SPIF);
	
	spi_read_buffer = (((uint16_t)SPDR) << 8) & 0xFF00;
	
	// Byte 2
	SPDR = (uint8_t)(data & 0x00FF);
	
	loop_until_bit_is_set(SPSR, SPIF);
	
	spi_read_buffer |= ((uint16_t)SPDR) & 0x00FF;
	
	// Return the enable pin to the inactive state
	*enable_port |= (1 << enable_pin);
	
	mutex = 0;
}

#endif

