/*
 *  spi.h
 *  MRF49XA
 *
 *  Created by William Dillon on 11/1/10.
 *  Copyright 2010 Oregon State University. All rights reserved.
 *
 */

#include <avr/io.h>
#include "hardware.h"

// Use these functions even with hardware SPI for portability
void spi_init(void);

// Read functions for 8 and 16 bit transfers
uint8_t  spi_read8(void);
uint16_t spi_read16(void);

// Write functions for 8 and 16 bit transfers
void spi_write8 (uint8_t  data, volatile uint8_t *enable_port, uint8_t enable_pin);
void spi_write16(uint16_t data, volatile uint8_t *enable_port, uint8_t enable_pin);
