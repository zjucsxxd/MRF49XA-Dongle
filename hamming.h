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

uint8_t  hamming_encode_nibble(uint8_t nibble);
uint16_t hamming_encode_byte(uint8_t  byte);

uint8_t  hamming_decode_nibble(uint8_t byte);
uint8_t  hamming_decode_byte(uint16_t symbol);

#endif
