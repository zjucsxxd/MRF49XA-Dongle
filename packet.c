//
//  packet.c
//  MRF49XA-Dongle
//
//  Created by William Dillon on 3/11/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#include "packet.h"
#include "modes.h"
#include "MRF49XA.h"
#include <LUFA/Drivers/USB/Class/Device/CDC.h>
#include <LUFA/Drivers/USB/USB.h>

extern USB_ClassInfo_CDC_Device_t CDC_interface;
extern volatile enum device_mode mode;

extern volatile uint8_t counter;
extern volatile MRF_packet_t packet;

void packetBreakReceived()
{
    return;
}

void packetMainLoop(void)
{
    return;
}
