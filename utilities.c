//
//  utilities.c
//  MRF49XA-Dongle
//
//  Created by William Dillon on 2/11/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#include "hardware.h"
#include "utilities.h"
#include <avr/wdt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <LUFA/Common/Common.h>
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Drivers/USB/Class/Device/CDC.h>

uint32_t Boot_Key ATTR_NO_INIT;
#define MAGIC_BOOT_KEY            0xDC42ACCA
#define BOOTLOADER_START_ADDRESS  0x3000

const uint8_t backSpaceString[] PROGMEM = "\x08 \x08";
const uint8_t tuningString[]    PROGMEM = "Tuned to: ";

extern uint8_t textBufferIndex;
extern uint8_t textBuffer[11];

extern USB_ClassInfo_CDC_Device_t CDC_interface;

void sendStringP(const uint8_t *string_in_progmem)
{
    while (pgm_read_byte(string_in_progmem) != '\0')
        CDC_Device_SendByte(&CDC_interface, pgm_read_byte(string_in_progmem++));
}

uint16_t stupid_divide(uint16_t dividend, uint16_t divisor, uint16_t *remainder)
{
    uint16_t quotient = 0;
    
    while (dividend >= divisor) {
        dividend -= divisor;
        quotient += 1;
    }
    
    if (remainder != NULL) {
        *remainder = dividend;
    }
    
    return quotient;
}

int8_t read_hex_value(uint8_t byte, uint16_t *result)
{
    // Keep accepting characters until a carriage return
    // or we've run out of room in the buffer.
    
    // If we're out of space, send a 'bell'
    if (textBufferIndex == 5) {
        CDC_Device_SendByte(&CDC_interface, 0x07);
        return 0;
    }
    
    // If we received a ctrl-c, exit
    if (byte == 0x03) {
        return -1;
    }
    
    // Capture whichever the first newline character
    // is first This indicates the end of the user input
    if (byte == '\n' || byte == '\r') {
        *result = 0;
        
        // Process each digit
        for (int i = 0; i < textBufferIndex; i++) {
            // Shift-over existing value
            *result = *result << 4;

            // Is this A-F?
            if (textBuffer[i] >= 'A' && textBuffer[i] <= 'F') {
                *result += textBuffer[i] - 'A' + 10;
            
            }
            
            // Is this 0-9?
            else if (textBuffer[i] >= '0' && textBuffer[i] <= '9') {
                *result += textBuffer[i] - '0';
            }
        }
        
        // Reset the index
        textBufferIndex = 0;
        
        return 1;
    }
    
    // Handle backspaces appropriately
    if (byte == 0x08) {
        // Make sure we don't backspace too far
        if (textBufferIndex == 0) {
            CDC_Device_SendByte(&CDC_interface, 0x07);
            return 0;
        }
        
        sendStringP(backSpaceString);
        textBufferIndex--;
        textBuffer[textBufferIndex] = '\0';
        return 0;
    }
    
    // Make every letter upper-case
    if (byte >= 'a' && byte <= 'z') {
        byte = byte - 32; // Crazy to-upper
    }
    
    // Filter out non-hex-digit input
    if ( (byte < '0') || (byte > '9' && byte < 'A') || (byte > 'F') ) {
        CDC_Device_SendByte(&CDC_interface, 0x07);
        return 0;
    }
    
    // Hopefully we've sanitized input enough
    CDC_Device_SendByte(&CDC_interface, byte);
    textBuffer[textBufferIndex] = byte;
    textBufferIndex++;
    return 0;
    

}

uint16_t print_digit(uint16_t value, uint16_t place)
{
    uint16_t remainder;
    uint16_t quotient = stupid_divide(value, place, &remainder);
    CDC_Device_SendByte(&CDC_interface, '0' + quotient);
    return remainder;
}

void print_dec(uint16_t value)
{
    uint16_t remainder = value;
    
    remainder = print_digit(remainder, 1000);
    remainder = print_digit(remainder,  100);
    remainder = print_digit(remainder,   10);
    remainder = print_digit(remainder,    1);
}

void print_digit_hex(uint8_t value)
{
    if (value > 9) {
        CDC_Device_SendByte(&CDC_interface, value - 10 + 'A');
    } else {
        CDC_Device_SendByte(&CDC_interface, value + '0');
    }
}

void print_hex(uint16_t value)
{
    CDC_Device_SendByte(&CDC_interface, '0');
    CDC_Device_SendByte(&CDC_interface, 'x');
    print_digit_hex((value & 0xF000) >> 12);
    print_digit_hex((value & 0x0F00) >>  8);
    print_digit_hex((value & 0x00F0) >>  4);
    print_digit_hex((value & 0x000F) >>  0);
}

void Bootloader_Jump_Check(void) ATTR_INIT_SECTION(3);
void Bootloader_Jump_Check(void)
{
    // If the reset source was the bootloader and the key is correct, clear it and jump to the bootloader
    if ((MCUSR & (1 << WDRF)) && (Boot_Key == MAGIC_BOOT_KEY))
    {
        Boot_Key = 0;
        ((void (*)(void))BOOTLOADER_START_ADDRESS)();
    }
}

// Functions to set hardware control line states
void setFlowControl_stop(void) {
    CDC_interface.State.ControlLineStates.DeviceToHost &= ~CDC_CONTROL_LINE_IN_DSR;
    CDC_Device_SendControlLineStateChange(&CDC_interface);
}

void setFlowControl_start(void) {
    CDC_interface.State.ControlLineStates.DeviceToHost |=  CDC_CONTROL_LINE_IN_DSR;
    CDC_Device_SendControlLineStateChange(&CDC_interface);
}

void jumpToBootloader(void)
{
    // If USB is used, detach from the bus and reset it
    USB_Disable();
    // Disable all interrupts
    cli();
    // Wait two seconds for the USB detachment to register on the host
    Delay_MS(2000);
    // Set the bootloader key to the magic value and force a reset
    Boot_Key = MAGIC_BOOT_KEY;
    wdt_enable(WDTO_250MS);
    for (;;);
}


