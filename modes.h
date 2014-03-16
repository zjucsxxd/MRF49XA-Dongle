//
//  modes.h
//  MRF49XA-Dongle
//
//  Created by William Dillon on 2/11/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#ifndef MRF49XA_Dongle_modes_h
#define MRF49XA_Dongle_modes_h

enum device_mode {
    INVALID    = 0,
    SERIAL     = 1,
    SERIAL_ECC = 2,
    PACKET     = 3,
    PACKET_ECC = 4,
    MENU       = 5,
    CAPTURE    = 6,
    TEST_ALT   = 7,
    TEST_ZERO  = 8,
    TEST_ONE   = 9,
    TEST_PING  = 10,
    USB_SERIAL = 11
};

#endif
