//
//  menu.c
//  MRF49XA-Dongle
//
//  Created by William Dillon on 2/11/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#include <avr/pgmspace.h>
#include <stdint.h>
#include "modes.h"
#include "utilities.h"
#include "registers.h"
#include "MRF49XA.h"
#include <LUFA/Drivers/USB/Class/Device/CDC.h>
#include <LUFA/Drivers/USB/USB.h>

extern USB_ClassInfo_CDC_Device_t CDC_interface;
extern volatile enum device_mode mode;

enum menu_item {
    MENU_TOP,
    MENU_EDIT,
    MENU_TEST,
    MENU_BOOT,
    MENU_EXIT };

volatile static enum menu_item menu = MENU_TOP;

#pragma mark PROGMEM strings
const uint8_t menuTopString[]   PROGMEM = "\n\r\
MRF49XA Dongle top menu\n\r\
1) Edit registers\n\r\
2) Test menu\n\r\
3) Enter transparent serial mode\n\r\
4) Enter packet serial mode\n\r\
5) Enter transparent serial mode with ECC\n\r\
6) Enter packet serial mode with ECC\n\r\
7x) Save boot state, set x to new start mode (0, 3, 4, 5 or 6 allowed)\n\r\
8) Firmware Upload (DFU)\n\r\
?) Print this menu\n\r\
> ";

const uint8_t menuEditString[]   PROGMEM = "\n\r\
MRF49XA Dongle register edit menu\n\r\
Edits the values for the device registers in memory\n\r\
Applies and saves the change immediately.\n\r\
Existing register contents:\n\r";

const uint8_t editIndexPromptString[] PROGMEM = "\n\r\
Enter register index to edit (non-number cancels): ";

const uint8_t editValuePromptString[] PROGMEM = "\n\r\
Enter new value for this register (Ctl-c cancels): 0x";

const uint8_t menuTestString[]   PROGMEM = "\n\r\
MRF49XA Dongle test menu\n\r\
1) TX alternating ones and zeros\n\r\
2) TX ones\n\r\
3) TX zeros\n\r\n\r\
4) Echo received packets\n\r\
5) Print received packets\n\r\n\r\
x) Stop function and exit\n\r\
?) Print this menu\n\r\
> ";

const uint8_t oldStartupString[]   PROGMEM = "Old startup Mode: ";
const uint8_t newStartupString[]   PROGMEM = "New startup Mode: ";
const uint8_t serialString[]    PROGMEM = "Transparent Serial";
const uint8_t packetString[]    PROGMEM = "Packet Serial";
const uint8_t menuString[]      PROGMEM = "Interactive";
const uint8_t ECCString[]       PROGMEM = " with ECC";
const uint8_t editString[]      PROGMEM = "Enter new value: 0x";
const uint8_t newLineString[]   PROGMEM = "\n\r";
const uint8_t invalidString[]   PROGMEM = "\n\rInvalid input ";
const uint8_t transmittingString[] PROGMEM = "\n\rTRANSMITTING!\n\r";

enum menu_item menuTopHandleByte(uint8_t byte);
enum menu_item menuEditHandleByte(uint8_t byte);
enum menu_item menuTestHandleByte(uint8_t byte);
enum menu_item menuBootHandleByte(uint8_t byte);

#pragma mark Menu logic
void menuHandleByte(uint8_t byte)
{
    // In menu mode, we need to maintain a state machine.
    // Most of the user interaction should be in the form
    // of single-byte commands.  The other case should be
    // simple numeric input.  For this case, we need to
    // maintain a string buffer for input and keep track
    // of what the meaning of that buffer is.  When a
    // carriage return occurs, we'll process the buffer
    // and take action.
    
    enum menu_item newLevel = menu;
    
    switch (menu) {
        case MENU_TOP:
            newLevel = menuTopHandleByte(byte);
            break;
        case MENU_EDIT:
            newLevel = menuEditHandleByte(byte);
            break;
        case MENU_TEST:
            newLevel = menuTestHandleByte(byte);
            break;
        case MENU_BOOT:
            newLevel = menuBootHandleByte(byte);
            break;
        default:
            newLevel = MENU_TOP;
            break;
    }
    
    if (newLevel == menu) {
        return;
    }
    
    menu = newLevel;
    switch (menu) {
        case MENU_TOP:
            sendStringP(newLineString);
            sendStringP(menuTopString);
            CDC_Device_Flush(&CDC_interface);
            break;
        case MENU_EDIT:
            sendStringP(newLineString);
            sendStringP(menuEditString);
            printSavedRegisters();
            sendStringP(editIndexPromptString);
            CDC_Device_Flush(&CDC_interface);
            break;
        case MENU_TEST:
            sendStringP(newLineString);
            sendStringP(menuTestString);
            CDC_Device_Flush(&CDC_interface);
            break;
        default:
            break;
    }
}

enum menu_item menuTopHandleByte(uint8_t byte)
{
/*
MRF49XA Dongle top menu
1) Edit registers
2) Test menu
3) Enter transparent serial mode
4) Enter packet serial mode
5) Enter transparent serial mode with ECC
6) Enter packet serial mode with ECC
7x) Save boot state, set x to new start mode (0, 3, 4, 5, or 6 allowed)
8) Firmware Upload (DFU)
?) Print this menu
>
*/
    switch (byte) {
        case '1':
            CDC_Device_SendByte(&CDC_interface, byte);
            return MENU_EDIT;
            
        case '2':
            CDC_Device_SendByte(&CDC_interface, byte);
            return MENU_TEST;
            
        case '3':
            CDC_Device_SendByte(&CDC_interface, byte);
            CDC_Device_Flush(&CDC_interface);
            mode = SERIAL;
            return MENU_EXIT;

        case '4':
            CDC_Device_SendByte(&CDC_interface, byte);
            CDC_Device_Flush(&CDC_interface);
            mode = PACKET;
            return MENU_EXIT;

        case '5':
            CDC_Device_SendByte(&CDC_interface, byte);
            CDC_Device_Flush(&CDC_interface);
            mode = SERIAL_ECC;
            return MENU_EXIT;
            
        case '6':
            CDC_Device_SendByte(&CDC_interface, byte);
            CDC_Device_Flush(&CDC_interface);
            mode = PACKET_ECC;
            return MENU_EXIT;
            
        case '7':
            CDC_Device_SendByte(&CDC_interface, byte);
            CDC_Device_Flush(&CDC_interface);
            return MENU_BOOT;

        case '8':
            jumpToBootloader();
            break;
            
        case '?':
            CDC_Device_SendByte(&CDC_interface, byte);
            sendStringP(newLineString);
            sendStringP(menuTopString);
            CDC_Device_Flush(&CDC_interface);
            break;
            
        case '\r':
            sendStringP(newLineString);
            CDC_Device_SendByte(&CDC_interface, '>');
        case '\n':
            break;
            
        default:
            CDC_Device_SendByte(&CDC_interface, byte);
            sendStringP(invalidString);
            sendStringP(menuTopString);
            CDC_Device_Flush(&CDC_interface);
            break;
    }
    
    return MENU_TOP;
}

enum menu_item menuEditHandleByte(uint8_t byte)
{
/*
MRF49XA Dongle register edit menu
Edits the values for the device registers in memory
Applies the change immediately, option 5 in top menu to save EEPROM.
Existing register contents:
*/

    // Get the entry to modify if necessary
    static int8_t index = -1;
    if(index < 0) {
        CDC_Device_SendByte(&CDC_interface, byte);

        if(byte < '0' || byte > '9') {
            return MENU_TOP;
        } else {
            index = byte - '0';
            sendStringP(editValuePromptString);
        }
        
        return MENU_EDIT;
    }
    
    // We have a selected index to modify
    uint16_t value;
    int8_t retval = read_hex_value(byte, &value);
    
    // Did we get a user cancel?
    if(retval < 0) {
        index = -1;
        return MENU_TOP;
    }
    
    // Are we still waiting for more input?
    if(retval == 0) {
        return MENU_EDIT;
    }

    // Has the user finished?
    if(retval > 0) {
        // Apply the change on the transceiver and save it in the eeprom
        setRegisterValue(index, value);

        // Reset the index
        index = -1;

        // Print the updated contents
        sendStringP(newLineString);
        sendStringP(menuEditString);
        printSavedRegisters();
        sendStringP(editIndexPromptString);
        CDC_Device_Flush(&CDC_interface);

        // Return to this menu (should it go to the main menu?)
        return MENU_EDIT;
    }
}

enum menu_item menuTestHandleByte(uint8_t byte)
{
/* MRF49XA Dongle test menu
1) Transmit alternating ones and zeros
2) Transmit ones
3) Transmit zeros
 
4) Echo received packets
5) Print received packets
6) Reset the transciever (stop transmitting)
x) Exit this menu (will stop testing function)
?) Print this menu
*/
    switch (byte) {
        case '1':
            CDC_Device_SendByte(&CDC_interface, byte);
            mode = TEST_ALT;
            MRF_transmit_alternating();
            sendStringP(transmittingString);
            break;
            
        case '2':
            CDC_Device_SendByte(&CDC_interface, byte);
            mode = TEST_ONE;
            MRF_transmit_one();
            sendStringP(transmittingString);
            break;
            
        case '3':
            CDC_Device_SendByte(&CDC_interface, byte);
            mode = TEST_ZERO;
            MRF_transmit_zero();
            sendStringP(transmittingString);
            break;
            
        case '4':
            CDC_Device_SendByte(&CDC_interface, byte);
            sendStringP(newLineString);
            CDC_Device_Flush(&CDC_interface);
            mode = TEST_PING;
            break;

        case '5':
            CDC_Device_SendByte(&CDC_interface, byte);
            sendStringP(newLineString);
            CDC_Device_Flush(&CDC_interface);
            mode = CAPTURE;
            break;

        case 'x':
            sendStringP(newLineString);
            CDC_Device_Flush(&CDC_interface);
            MRF_reset();
            mode = MENU;
            return MENU_TOP;
            break;

        case '?':
            CDC_Device_SendByte(&CDC_interface, byte);
            sendStringP(menuTestString);
            CDC_Device_Flush(&CDC_interface);
            break;

        case '\r':
            sendStringP(newLineString);
            CDC_Device_SendByte(&CDC_interface, '>');
        case '\n':
            break;
            
        default:
            CDC_Device_SendByte(&CDC_interface, byte);
            sendStringP(invalidString);
            sendStringP(menuTestString);
            CDC_Device_Flush(&CDC_interface);
            break;
    }
    
    return MENU_TEST;
}

enum menu_item menuBootHandleByte(uint8_t byte)
{
    CDC_Device_SendByte(&CDC_interface, byte);
    CDC_Device_Flush(&CDC_interface);

    sendStringP(newLineString);
    sendStringP(oldStartupString);
    
    // Print the existing boot setting
    switch(getBootState()) {
        case SERIAL:
            sendStringP(serialString);
        case SERIAL_ECC:
            sendStringP(ECCString);
            break;
        case PACKET:
            sendStringP(packetString);
        case PACKET_ECC:
            sendStringP(ECCString);
            break;
        case MENU:
            sendStringP(menuString);
            break;
    }
    
    sendStringP(newLineString);
    sendStringP(newStartupString);

    // This should be a valid new startup mode:
    switch(byte) {
        case '0':
            sendStringP(menuString);
            setBootState(MENU);
            break;

        case '3':
            sendStringP(serialString);
            setBootState(SERIAL);
            break;

        case '4':
            sendStringP(serialString);
            sendStringP(ECCString);
            setBootState(SERIAL_ECC);
            break;

        case '5':
            sendStringP(packetString);
            setBootState(PACKET);
            break;

        case '6':
            sendStringP(packetString);
            sendStringP(ECCString);
            setBootState(PACKET_ECC);
            break;

        default:
            sendStringP(invalidString);
            return MENU_TOP;
    }
    
    return MENU_TOP;
}