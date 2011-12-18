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
    
    CDC_Device_SendControlLineStateChange(&CDC_interface);
}

void setFlowControl_start(void) {
    CDC_interface.State.ControlLineStates.DeviceToHost |=  CDC_CONTROL_LINE_IN_DSR;
    
    CDC_Device_SendControlLineStateChange(&CDC_interface);    
}

// Basic callbacks for USB events.
void EVENT_USB_Device_Connect(void)
{
    LED_PORTx = LEDS_ENUM;
}

void EVENT_USB_Device_Disconnect(void)
{
    configured = 0;
    LED_PORTx = LEDS_ERROR;
}

void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool success = CDC_Device_ConfigureEndpoints(&CDC_interface);
    
    if (success) {
        configured = 1;
        LED_PORTx = LEDS_READY;
        
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
    CDC_Device_SendByte(&CDC_interface, byte);
    LED_PORTx ^= LED_POWER;
    
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
            packet.payload[counter - 2] = byte;
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
