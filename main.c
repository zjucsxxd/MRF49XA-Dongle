//
//  main.c
//  MRF49XA-Dongle
//
//  Created by William Dillon on 11/23/11.
//  Copyright (c) 2011. Licensed under the GPL v. 2
//  Uses code licensed under the MIT and LGPL by file:
//
//  Descriptors.c and .h: Dean Camera (MIT Licensed)
//  rs8.c and rs8.h: Phil Karn (LGPL Licensed)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to:
// Free Software Foundation, Inc.
// 51 Franklin Street, Fifth Floor
// Boston, MA  02110-1301, USA.
//

#include "MRF49XA.h"
#include "hardware.h"
#include "rs8.h"
#include "spi.h"

#include <avr/wdt.h>
#include <avr/sfr_defs.h>
#include <util/delay.h>
#include <avr/power.h>

#include "Descriptors.h"
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Drivers/USB/Class/Device/CDC.h>

#define LEDS_INIT   ((1 << LED_POWER) | (1 << LED_RX) | (1 << LED_TX))
#define LEDS_ENUM   ((1 << LED_POWER) | (1 << LED_RX))
#define LEDS_READY   (1 << LED_POWER)
#define LEDS_ERROR  ((1 << LED_POWER) | (1 << LED_TX))

static MRF_packet_t packet;
// This should be volatile because it can be changed in the break event
volatile static int counter = 0;

/** LUFA CDC Class driver interface configuration and state information.
 *  This structure is passed to all CDC Class driver functions, so that
 *  multiple instances of the same class within a device can be differentiated
 *  from one another.  (Copied from the LUFA VirtualSerial.c example)
 */
USB_ClassInfo_CDC_Device_t CDC_interface =
{
    .Config =
    {
        .ControlInterfaceNumber         = 0,
        
        .DataINEndpointNumber           = CDC_TX_EPNUM,
        .DataINEndpointSize             = CDC_TXRX_EPSIZE,
        .DataINEndpointDoubleBank       = false,
        
        .DataOUTEndpointNumber          = CDC_RX_EPNUM,
        .DataOUTEndpointSize            = CDC_TXRX_EPSIZE,
        .DataOUTEndpointDoubleBank      = false,
        
        .NotificationEndpointNumber     = CDC_NOTIFICATION_EPNUM,
        .NotificationEndpointSize       = CDC_NOTIFICATION_EPSIZE,
        .NotificationEndpointDoubleBank = false,
    },
};

// This is the "file" representing the USB stream
// it is non-blocking, so line-based io is unlikely to work as expected.
static FILE USB_USART;
bool configured;

#pragma mark USB logic
// Functions to set hardware control line states
void setFlowControl_stop(void) {    
    CDC_interface.State.ControlLineStates.DeviceToHost &= ~CDC_CONTROL_LINE_IN_DSR;
    LED_PORTx |= (1 << LED_TX);
    LED_PORTx |= (1 << LED_POWER);

    CDC_Device_SendControlLineStateChange(&CDC_interface);
}

void setFlowControl_start(void) {
    CDC_interface.State.ControlLineStates.DeviceToHost |=  CDC_CONTROL_LINE_IN_DSR;
    LED_PORTx &= ~(1 << LED_TX);    
    LED_PORTx &= ~(1 << LED_POWER);

    CDC_Device_SendControlLineStateChange(&CDC_interface);    
}

// Basic callbacks for USB events.
void EVENT_USB_Device_Connect(void)
{
//    LED_PORTx = LEDS_ENUM;
}

void EVENT_USB_Device_Disconnect(void)
{
    configured = 0;
//    LED_PORTx = LEDS_ERROR;
}

void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool success = CDC_Device_ConfigureEndpoints(&CDC_interface);
    
    if (success) {
        configured = 1;
//        LED_PORTx = LEDS_READY;
        
        // Set HW flow control to allow communication
        setFlowControl_start();
    } else {
        configured = 0;
    }
}

void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&CDC_interface);
}

void EVENT_USB_DEVICE_ConfigureEndpoints(void)
{
    CDC_Device_ConfigureEndpoints(&CDC_interface);
}

void EVENT_CDC_BreakSent(USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo,
                         const uint8_t Duration)
{
    // The break event has the effect of transmitting the current buffer
    // regardless of the system state.  This can be used to force bad packets
    // or to reset the state machine
    MRF_transmit_packet(&packet);
    counter = 0;
}

#pragma mark Application logic

char generator[16] PROGMEM = {
    0x00,
    0x17,
    0x2B,
    0x3C,
    0x4D,
    0x5A,
    0x66,
    0x71,
    0x8E,
    0x99,
    0xA5,
    0xB2,
    0xC3,
    0xD4,
    0xE8,
    0xFF};

// This table of values maps codewords back to data.  It's organized into 16
// rows of 16 columns. Because we're using a "systematic" code, which means
// that the data nibble is available as the first part of the codeword, the
// current table is organized into rows of the same (uncorrected) data nibble.
// I've decided to let the structure strongly prefer the rows over the columns.
// To get an idea of the scale of the effect, I've counted the "value" part of
// the returns here (low bits) for each.
//     The rows contain 12 instances of each value, and the columns have 5.
// Ideally, there would be a roughly equal number in the columns and rows.
// This would mean that the error correction isn't biased against the parity
// part of the codeword.  Really, I'm playing games here, because the code
// is only a one-error correcting code, so there would be lots of blanks in
// the table. I don't have a way to represent this, and I don't have an
// out-of-band "erasure" signal, so I just replace it with a plausible response
// that might be wrong, but it might be right, too.  Ultimately, we're still
// only getting our one guarenteed correction plus some speculative responses.
//     If there is some way to mark "erasures", such as using the ASCII "null"
// character we could depend on the higher-level protocol to do some correction.
// For example, using NMEA-0183 sentences (GPS), in the case of a single
// erasure, the lost character could be recovered using the checksum.  We can
// extract erasures from this table by examining the symptom word (higher-order
// nibble).  If the symptom has 2 bits, it's a double-bit error and we're not
// confident in its value.  We cannot distinguish between 1 and 3 bit errors,
// and they will be blindly corrected.
char check[256] PROGMEM = {
    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x71, 0x80, 0x90, 0xa0, 0xb2, 0xc0, 0xd4, 0xe8, 0xf0, // 12 0's
    0x70, 0x61, 0x51, 0x41, 0x31, 0x21, 0x11, 0x01, 0xf1, 0xe9, 0xd5, 0xc1, 0xb3, 0xa1, 0x91, 0x81, // 12 1's
    0xb0, 0xa2, 0x92, 0x82, 0xf2, 0xea, 0xd6, 0xc2, 0x32, 0x22, 0x12, 0x02, 0x73, 0x62, 0x52, 0x42, // 12 2's
    0xc3, 0xd7, 0xeb, 0xf3, 0x83, 0x93, 0xa3, 0xb1, 0x43, 0x53, 0x63, 0x72, 0x03, 0x13, 0x23, 0x33, // 12 3's
    0xd0, 0xc4, 0xf4, 0xec, 0x94, 0x84, 0xb6, 0xa4, 0x54, 0x44, 0x75, 0x64, 0x14, 0x04, 0x34, 0x24, // 12 4's
    0xa5, 0xb7, 0x85, 0x95, 0xed, 0xf5, 0xc5, 0xd1, 0x25, 0x35, 0x05, 0x15, 0x65, 0x74, 0x45, 0x55, // 12 5's
    0x66, 0x77, 0x46, 0x56, 0x26, 0x36, 0x06, 0x16, 0xee, 0xf6, 0xc6, 0xd2, 0xa6, 0xb4, 0x86, 0x96, // 12 6's
    0x17, 0x07, 0x37, 0x27, 0x57, 0x47, 0x76, 0x67, 0x97, 0x87, 0xb5, 0xa7, 0xd3, 0xc7, 0xf7, 0xef, // 12 7's
    0xe0, 0xf8, 0xc8, 0xdc, 0xa8, 0xba, 0x88, 0x98, 0x68, 0x79, 0x48, 0x58, 0x28, 0x38, 0x08, 0x18, // 12 8's
    0x99, 0x89, 0xbb, 0xa9, 0xdd, 0xc9, 0xf9, 0xe1, 0x19, 0x09, 0x39, 0x29, 0x59, 0x49, 0x78, 0x69, // 12 9's
    0x5a, 0x4a, 0x7b, 0x6a, 0x1a, 0x0a, 0x3a, 0x2a, 0xde, 0xca, 0xfa, 0xe2, 0x9a, 0x8a, 0xb8, 0xaa, // 12 a's
    0x2b, 0x3b, 0x0b, 0x1b, 0x6b, 0x7a, 0x4b, 0x5b, 0xab, 0xb9, 0x8b, 0x9b, 0xe3, 0xfb, 0xcb, 0xdf, // 12 b's
    0x3c, 0x2c, 0x1c, 0x0c, 0x7d, 0x6c, 0x5c, 0x4c, 0xbe, 0xac, 0x9c, 0x8c, 0xfc, 0xe4, 0xd8, 0xcc, // 12 c's
    0x4d, 0x5d, 0x6d, 0x7c, 0x0d, 0x1d, 0x2d, 0x3d, 0xcd, 0xd9, 0xe5, 0xfd, 0x8d, 0x9d, 0xad, 0xbf, // 12 d's
    0x8e, 0x9e, 0xae, 0xbc, 0xce, 0xda, 0xe6, 0xfe, 0x0e, 0x1e, 0x2e, 0x3e, 0x4e, 0x5e, 0x6e, 0x7f, // 12 e's
    0xff, 0xe7, 0xdb, 0xcf, 0xbd, 0xaf, 0x9f, 0x8f, 0x7e, 0x6f, 0x5f, 0x4f, 0x3f, 0x2f, 0x1f, 0x0f  // 12 f's
//  5 0s, 5 7s, 5 Bs, 5 Cs, 5 Ds, 5 As, 5 6s, 5 1s, 5 Es, 5 9s, 5 5s, 5 2s, 5 3s, 5 4s, 5 8s, 5 Fs
};

void
init(void) {
    configured = 0;

    // Enable some LEDs, just to make sure it loads
    LED_DDRx = (1 << LED_POWER) | (1 << LED_RX) | (1 << LED_TX);

    // Disable watchdog
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
    
	// Disable prescaler (the CLKDIV8 fuse is set at the factory)
    clock_prescale_set(clock_div_1);

    // Initialize the SPI bus
    spi_init();
    
    // Initialize the transceiver
    MRF_init();
    
    // Initialize the USB system
	USB_Init();
    
    // Create the CDC serial stream device
    CDC_Device_CreateStream(&CDC_interface, &USB_USART);
    
    // Enable interrupts
    sei();    
}

void byteReceved(char byte)
{
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
            packet.payload[(counter - 2) * 2 + 0] = generator[(byte >> 4) & 0xF];
            packet.payload[(counter - 2) * 2 + 1] = generator[byte & 0xF];
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

int
main(void) {
    
    init();

    while (true) {
        if (CDC_Device_BytesReceived(&CDC_interface) > 0) {
            byteReceved(CDC_Device_ReceiveByte(&CDC_interface));
        }
        
        // Test for a new packet.  If there is one, send it along over the USB
        // If the USB isn't configured, drop it on the floor
        MRF_packet_t *rx_packet = MRF_receive_packet();
        if (rx_packet  != NULL &&
            configured == true) {
            CDC_Device_SendByte(&CDC_interface, rx_packet->payloadSize);
            CDC_Device_SendByte(&CDC_interface, rx_packet->type);
            
            // Decode the hamming code ECC.  Basically, we're going to
            // scan through each byte in the received data, and look it
            // up in the decode table.  The Higher-order nibble is the
            // error-corrected data.
//            char i;
//            for (i = 0; i < rx_packet->payloadSize; i++) {
//                
//            }
            
            CDC_Device_SendData(&CDC_interface,
                                rx_packet->payload,
                                rx_packet->payloadSize);
        }

        // Both of these functions must be run at least once every 30mS
        // Using the INTERRUPT_CONTROL_ENDPOINT flag,
        // we're doing this work in an interrupt now, so we don't need this.
        CDC_Device_USBTask(&CDC_interface);
        USB_USBTask();
    }
}
