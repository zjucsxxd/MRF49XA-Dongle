//
//  utilities.h
//  MRF49XA-Dongle
//
//  Created by William Dillon on 2/11/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#ifndef MRF49XA_Dongle_utilities_h
#define MRF49XA_Dongle_utilities_h

#include <stdint.h>

void    sendStringP(const uint8_t *string_in_progmem);

int8_t  read_dec_value(uint8_t byte, uint16_t *result);
int8_t  read_hex_value(uint8_t byte, uint16_t *result);

void print_hex(uint16_t value);
void print_dec(uint16_t value);

void jumpToBootloader(void);

void setFlowControl_start(void);
void setFlowControl_stop(void);

#endif
