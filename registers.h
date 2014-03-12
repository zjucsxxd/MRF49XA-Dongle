//
//  EEPROM.h
//  MRF49XA-Dongle
//
//  Created by William Dillon on 3/7/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#ifndef MRF49XA_Dongle_EEPROM_h
#define MRF49XA_Dongle_EEPROM_h

#include <stdint.h>

void applySavedRegisters(void);
void printSavedRegisters(void);
void setRegisterValue(uint8_t index, uint16_t value);

uint8_t getBootState(void);
void    setBootState(uint8_t bootMode);

#endif
