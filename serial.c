//
//  serial.c
//  MRF49XA-Dongle
//
//  Created by William Dillon on 3/11/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#include "serial.h"
#include "modes.h"
#include "MRF49XA.h"
#include "hamming.h"
#include <LUFA/Drivers/USB/Class/Device/CDC.h>
#include <LUFA/Drivers/USB/USB.h>

extern USB_ClassInfo_CDC_Device_t CDC_interface;
extern volatile enum device_mode mode;

extern volatile uint8_t counter;
extern volatile MRF_packet_t packet;

// Byte received from the USB port
void byteReceved(uint8_t byte)
{
    uint16_t *symbols = (uint16_t *)packet.payload;
//    CDC_Device_SendByte(&CDC_interface, byte);
    
    // Fill out the packet contents
    switch (counter) {
        case 0:
            // Sanity checking on the length byte
            if (byte <= MRF_PAYLOAD_LEN) {
                packet.payloadSize = byte;
                counter++;
            }
            break;
        case 1:
            packet.type = byte;
            counter++;
            break;
        default:
            // Encode the data using the Hamming code ECC
            symbols[counter - 2] = hamming_encode(byte);
            counter++;
            break;
    }
    
    // If the counter equals the packet size, transmit
    if (counter >= packet.payloadSize + MRF_PACKET_OVERHEAD) {
        // Disable new serial data during this routine
        setFlowControl_stop();
        MRF_transmit_packet(&packet);
        setFlowControl_start();
        
        counter = 0;
    }
    
}

void serialBreakReceived()
{
    // The break event has the effect of transmitting the current buffer
    // regardless of the system state.  This can be used to force bad packets
    // or to reset the state machine
    MRF_transmit_packet(&packet);
    counter = 0;
}

void serialMainLoop(void)
{
    return;
}

