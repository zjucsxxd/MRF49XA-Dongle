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
#include "utilities.h"
#include <LUFA/Drivers/USB/Class/Device/CDC.h>
#include <LUFA/Drivers/USB/USB.h>

extern USB_ClassInfo_CDC_Device_t CDC_interface;
extern volatile enum device_mode mode;

extern volatile uint8_t counter;
extern volatile MRF_packet_t packet;

// Send a partially-filled packet if we don't receive a byte in 1/10 of a second
#define TICKS_BYTE_DEADLINE 36
extern volatile uint16_t ticks;

void serialTransmitPacket(void)
{
    // Disable new serial data during this routine
    setFlowControl_stop();
    
    packet.payloadSize = counter;
    if (mode == SERIAL_ECC) {
        packet.type = PACKET_TYPE_SERIAL_ECC;
    } else {
        packet.type = PACKET_TYPE_SERIAL;
    }
    
    MRF_transmit_packet(&packet);
    counter = 0;

    setFlowControl_start();
}

// Byte received from the USB port
void serialByteReceved(uint8_t byte)
{
    packet.payload[counter++] = byte;
    
    // Are we full yet?
    if (counter == MRF_PAYLOAD_LEN) {
        serialTransmitPacket();
    }
}

void serialBreakReceived()
{
    // If a break is sent, it's possible to send the packet immediately.
    serialTransmitPacket;
}

void serialMainLoop(void)
{
    static uint16_t sendDeadline = 0;
    
    // Handle new bytes from USB
    if (CDC_Device_BytesReceived(&CDC_interface) > 0) {
        sendDeadline = ticks + TICKS_BYTE_DEADLINE;
        serialByteReceved(CDC_Device_ReceiveByte(&CDC_interface));
    }
    
    // If we've waited too long for a new byte send what we have so far
    if (counter > 0) {
        if (ticks > sendDeadline) {
            serialTransmitPacket();
        }
    }
    
    // Handle new packets from radio
    MRF_packet_t *rx_packet = MRF_receive_packet();
    if (rx_packet) {
        // Print the packet contents directly to USB
        CDC_Device_SendData(&CDC_interface,
                            rx_packet->payload,
                            rx_packet->payloadSize);
    }
    
    return;
}

