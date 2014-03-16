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

#include "modes.h"
#include "MRF49XA.h"
#include "hardware.h"
#include "spi.h"
#include "menu.h"
#include "hamming.h"
#include "utilities.h"
#include "Descriptors.h"
#include "registers.h"
#include "packet.h"
#include "serial.h"
#include "usbSerial.h"

#include <avr/wdt.h>
#include <avr/sfr_defs.h>
#include <util/delay.h>
#include <avr/power.h>

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Drivers/USB/Class/Device/CDC.h>

#pragma mark Globals and constants
#define LEDS_INIT   ((1 << LED_POWER) | (1 << LED_RX) | (1 << LED_TX))
#define LEDS_ENUM   ((1 << LED_POWER) | (1 << LED_RX))
#define LEDS_READY   (1 << LED_POWER)
#define LEDS_ERROR  ((1 << LED_POWER) | (1 << LED_TX))

// This should be volatile because it can be changed in the break event
volatile uint8_t counter = 0;
volatile MRF_packet_t packet;

// A shared text buffer
volatile uint8_t textBufferIndex = 0;
volatile uint8_t textBuffer[11];

// Global variable for holding the device mode
volatile enum device_mode mode = MENU;

volatile uint16_t ticks = 0;

#pragma mark USB logic
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

// This ISR is responsible for running the USB code.  It MUST run at least
// every 30mS.  I'm probably going to start running it twice that often.
ISR(TIMER0_OVF_vect, ISR_NOBLOCK)
{
    // Increment the ticks counter
    // which will overflow once every 536 seconds, or 8.94 minutes.
    ticks++;
    
    CDC_Device_USBTask(&CDC_interface);
    USB_USBTask();
}

// This is the "file" representing the USB stream
// it is non-blocking, so line-based io is unlikely to work as expected.
static FILE USB_USART;
bool configured;

// Basic callbacks for USB events.
void EVENT_USB_Device_Connect(void)
{
    LED_PORTx = (1 << LED_POWER);
}

void EVENT_USB_Device_Disconnect(void)
{
    configured = 0;
}

void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool success = CDC_Device_ConfigureEndpoints(&CDC_interface);
    
    if (success) {
        configured = 1;
        
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

// Lets try to interpret a break as a request to enter the menu
void EVENT_CDC_BreakSent(USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo, const uint8_t Duration)
{
    mode = MENU;
}

#pragma mark Application logic

void
init(void) {
    configured = 0;

    // Start with all pullups off, and all ports input
    DDRB  = 0x00;
    DDRC  = 0x00;
    DDRD  = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;
    
    // Enable some LEDs, just to make sure it loads
    LED_DDRx  = (1 << LED_POWER) | (1 << LED_RX) | (1 << LED_TX);
    LED_PORTx = LEDS_INIT;
    
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
    
    // Begin the interrupt-based USB checking
    TCCR0A = 0x00; // Disable compare A, B and the PWM
    TCCR0B = 0x04; // Disable force compare, enable divide-by 256 prescaler (31,250 Hz)
    // With the 256 overflow (8-bit) the 31,250 Hz base clock will yield a 122 Hz rate
    // this should be sufficient for the 30 mS rate required by USB (actual is 8mS).
    TIMSK0 = 0x01; // Enable the overflow timer interrupt
    
    // Create the CDC serial stream device
    CDC_Device_CreateStream(&CDC_interface, &USB_USART);
    
    // Setup the internal UART
    // Setup the DDRD for RXD and TXD
    DDRD &= ~(1 << 2);
    DDRD |=  (1 << 3);
    
    // set baud rate (8000000 / (16 * 4800)) - 1 = 103, for 4800 baud
    UBRR1H = 0;
    UBRR1L = 103;
    
    // Enable UART receiver and transmitter.
    UCSR1B = (1 << RXEN1) | (1 << TXEN1);
    
    // Set frame format: asynchronous, 8data, no parity, 1stop bit
    UCSR1C = (1 << UCSZ11)| (1 << UCSZ10);
    
    // Enable interrupts
    sei();
}

const uint8_t pingString[]          PROGMEM = "PINGING: ";

const uint8_t typeSerialString[]    PROGMEM = "\n\rSerial, ";
const uint8_t typeSerialECCString[] PROGMEM = "\n\rSerial ECC, ";
const uint8_t typePacketString[]    PROGMEM = "\n\rPacket, ";
const uint8_t typePacketECCString[] PROGMEM = "\n\rPacket ECC, ";
const uint8_t typeUnknownString[]   PROGMEM = "\n\rUnknown type, ";
const uint8_t packetLengthString[]  PROGMEM = "length ";
const uint8_t invalidModeString[]   PROGMEM = " Invalid mode, ";

void printPacket(MRF_packet_t *rx_packet)
{
    // Print a label for the packet type
    switch (rx_packet->type) {
        case PACKET_TYPE_SERIAL:
            sendStringP(typeSerialString);
            break;
        case PACKET_TYPE_SERIAL_ECC:
            sendStringP(typeSerialECCString);
            break;
        case PACKET_TYPE_PACKET:
            sendStringP(typePacketString);
            break;
        case PACKET_TYPE_PACKET_ECC:
            sendStringP(typePacketECCString);
            break;
        default:
            sendStringP(typeUnknownString);
            break;
    }
    
    // Print the packet length
    sendStringP(packetLengthString);
    print_dec(rx_packet->payloadSize);
    CDC_Device_SendByte(&CDC_interface, ':');
    CDC_Device_SendByte(&CDC_interface, ' ');
    
    // Print the packet contents
    for (int i = 0; i < rx_packet->payloadSize; i++) {
        if (i % 10 == 0) {
            CDC_Device_SendByte(&CDC_interface, '\n');
            CDC_Device_SendByte(&CDC_interface, '\r');
        }
        
        uint8_t byte = rx_packet->payload[i];
        CDC_Device_SendByte(&CDC_interface, byte);
//        if (byte & 0xF0 > 0x90) {
//            CDC_Device_SendByte(&CDC_interface, ((byte & 0xF0) >> 4) + 'A');
//        } else {
//            CDC_Device_SendByte(&CDC_interface, ((byte & 0xF0) >> 4) + '0');
//        }
//        
//        if (byte & 0x0F > 0x09) {
//            CDC_Device_SendByte(&CDC_interface, (byte & 0x0F) + 'A');
//        } else {
//            CDC_Device_SendByte(&CDC_interface, (byte & 0x0F) + '0');
//        }
    }
    
    CDC_Device_Flush(&CDC_interface);
}

int main(void) {
    uint8_t byte;
    
    // Initalize the system
    init();
    
    // Load the saved registers
    applySavedRegisters();
    MRF_reset();
    
    // Get the default boot state
    mode = getBootState();
    
    // Loop here forever
    while (true) {
        
        // Process menu actions as long as we're in the menu or test modes
        // New packets received while in menu mode are ignored
        if (mode == MENU      ||
            mode == TEST_ALT  ||
            mode == TEST_ZERO ||
            mode == TEST_ONE  ||
            mode == CAPTURE   ||
            mode == TEST_PING ) {

            // Handle any new bytes from the USB system
            // These functions can be assumed to return immediately.
            if (CDC_Device_BytesReceived(&CDC_interface) > 0) {
                byte = CDC_Device_ReceiveByte(&CDC_interface);

                menuHandleByte(byte);
            }
            
            // Test for a new packet
            MRF_packet_t *rx_packet = MRF_receive_packet();

            // Was the packet correctly received?
            if (rx_packet  != NULL) {
                switch (mode) {
                    case TEST_PING:
                        MRF_transmit_packet(rx_packet);
                        sendStringP(pingString);
                        printPacket(rx_packet);
                        break;
                    case CAPTURE:
                        printPacket(rx_packet);
                        break;
                    default:
                        // Other menu modes would end up here, ignore the packet
                        break;
                }
            }
        }
        
        // These modes are responsible for actually using the RF interface
        else {
            switch (mode) {
                case PACKET:
                case PACKET_ECC:
                    packetMainLoop();
                    break;
                    
                case SERIAL:
                case SERIAL_ECC:
                    serialMainLoop();
                    break;
                    
                case USB_SERIAL:
                    usbSerialMainLoop();
                    break;
                    
                default:
                    // This would catch any weird modes
                    sendStringP(invalidModeString);
                    print_dec(mode);
                    CDC_Device_Flush(&CDC_interface);
                    mode = MENU;
                    break;
            }
        }
    }
}
