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

#define LED1_RED   (1 << 4)
#define LED1_GREEN (1 << 5)
#define LED2_RED   (1 << 7)
#define LED2_GREEN (1 << 6)

#define LEDS_INIT   (LED1_RED   | LED2_RED | LED2_GREEN)
#define LEDS_ENUM   (LED1_RED   | LED2_GREEN)
#define LEDS_READY  (LED1_GREEN | LED2_GREEN)
#define LEDS_ERROR  (LED1_RED   | LED1_RED)

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

#pragma mark USB Event callbacks
// Basic callbacks for basic USB events.
// For now, we'll change the status of the USB Key LEDs
// as a basic form of debugging.
void EVENT_USB_Device_Connect(void)
{
    PORTD = LEDS_ENUM;
}

void EVENT_USB_Device_Disconnect(void)
{
    PORTD = LEDS_ERROR;
}

void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool success = CDC_Device_ConfigureEndpoints(&CDC_interface);
    
    if (success) {
        PORTD = LEDS_READY;
    } else {
        PORTD = LEDS_ERROR;       
    }
}

void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&CDC_interface);
}

#pragma mark Data management functions

bool byteFromUSB(char byte) {
    static char inputCounter = 0;
    static MRF_packet_t packet;
    
    // Toggle the LED (for debugging
    PORTD = PORTD | LED1_RED;
    
    // Copy the byte into the packet
    packet.payload[inputCounter] = byte;
    
    // Increment the counter
    inputCounter++;
    
    // If the counter equals the maximum payload size,
    // calculate the FEC values and 
    // send the packet along to the transmitter
    if (inputCounter == MRF_PAYLOAD_LEN) {

        // Encode the data using the RS 8 encoder
        encode_rs_8(packet.payload,
                    packet.FEC, 0);
        PORTD = PORTD & ~LED1_RED;

        // Set the payload size field
        packet.payloadSize = inputCounter;
        
        // Send the packet to the RF stack
        MRF_transmit_packet(&packet);
        
        inputCounter = 0;
        return true;
    } else {
        return false;
    }
}

void
init(void) {
    // Enable some LEDs, just to make sure it loads
    // Other than the LEDs, port D is unused
    DDRD  = 0xFF;
    
    PORTD = LEDS_INIT;
    
    // Disable watchdog
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
    
	// Disable prescaler (the CLKDIV8 fuse is set at the factory)
	clock_prescale_set(clock_div_1);
    
    // Initialize the USB system
	USB_Init();
    
    // Create the CDC serial stream device
    CDC_Device_CreateStream(&CDC_interface, &USB_USART);
    
    // Initialize the SPI bus
    spi_init();
    
    // Initialize the transceiver
    MRF_init();
    
    // Enable interrupts
    sei();
}

int
main(void) {
    char byte;
    char count;
    
    init();
    
    // Main loop
    while (true) {
        count = CDC_Device_BytesReceived(&CDC_interface);
        
        for (int i = 0; i < count; i++) {
            byte = CDC_Device_ReceiveByte(&CDC_interface);

            // Echo the byte back right away
            CDC_Device_SendData(&CDC_interface, &byte, 1);
            
            byteFromUSB(byte);
        }

        // Both of these functions must be run at least once every 30mS
        // Using the INTERRUPT_CONTROL_ENDPOINT flag,
        // we're doing this work in an interrupt now, so we don't need this.
        CDC_Device_USBTask(&CDC_interface);
        USB_USBTask();
    }
}
