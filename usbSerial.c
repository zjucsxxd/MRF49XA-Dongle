//
//  usbSerial.c
//  MRF49XA-Dongle
//
//  Created by William Dillon on 3/15/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#include "usbSerial.h"
#include "modes.h"
#include "utilities.h"
#include <LUFA/Drivers/USB/Class/Device/CDC.h>
#include <LUFA/Drivers/USB/USB.h>

extern USB_ClassInfo_CDC_Device_t CDC_interface;
extern volatile enum device_mode mode;

void uartSend(uint8_t data)
{
    while ( !( UCSR1A & (1 << UDRE1)) );
    UDR1 = data;
}

void usbSerialMainLoop(void)
{
    UDR1 = 0xAA;
    
    // Handle new bytes from USB
    if (CDC_Device_BytesReceived(&CDC_interface) > 0) {
        uartSend(CDC_Device_ReceiveByte(&CDC_interface));
    }
    
    // Handle new packets from USART
    if (UCSR1A & (1 << RXC1)) {
        CDC_Device_SendByte(&CDC_interface, UDR1);
    }
    
    return;
}

