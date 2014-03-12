//
//  hamming.h
//  MRF49XA-Dongle
//
//  Created by William Dillon on 2/11/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#ifndef MRF49XA_Dongle_hamming_h
#define MRF49XA_Dongle_hamming_h

#include <stdint.h>

uint16_t hamming_encode(uint8_t  byte);
uint8_t  hamming_decode(uint16_t symbol);

#endif
