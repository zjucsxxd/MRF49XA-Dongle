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
#include "utilities.h"
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

void packetByteReceived(uint8_t byte)
{
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
            
            if (mode == SERIAL_ECC) {
                packet.type = PACKET_TYPE_SERIAL_ECC;
            } else {
                packet.type = PACKET_TYPE_SERIAL;
            }
            
            counter++;
            break;
            
        default:
            packet.payload[counter - 2] = byte;
            counter++;
            break;
    }
    
    // If the counter equals the packet size, transmit
    if (counter >= packet.payloadSize + MRF_PACKET_OVERHEAD) {
        // Disable new serial data during this routine
        MRF_transmit_packet(&packet);
        counter = 0;
    }

}

void packetMainLoop(void)
{
    // Handle new bytes from USB
    if (CDC_Device_BytesReceived(&CDC_interface) > 0) {
        packetByteReceived(CDC_Device_ReceiveByte(&CDC_interface));
    }

    return;
}
