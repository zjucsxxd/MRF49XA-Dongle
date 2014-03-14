//
//  EEPROM.c
//  MRF49XA-Dongle
//
//  Created by William Dillon on 3/7/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#include <avr/eeprom.h>
#include "hamming.h"
#include "registers.h"
#include "modes.h"
#include "MRF49XA.h"
#include "utilities.h"
#include <LUFA/Drivers/USB/Class/Device/CDC.h>
extern USB_ClassInfo_CDC_Device_t CDC_interface;

// The first EEPROM entry is likely to be corrupted, don't use it.
//uint8_t EEMEM likelyToBeGarbage;

// This entry contain persistent device mode hamming ECC encoded
// The default is to have an invalid mode, which will force reloading
uint8_t EEMEM blank[512];

// These registers are fully under user control
/*
uint16_t EEMEM afcreg      = 0xC4F7;
uint16_t EEMEM txcreg      = 0x9810;
uint16_t EEMEM cfsreg      = 0xA348;
uint16_t EEMEM rxcreg      = 0x94C0;
uint16_t EEMEM bbfcreg     = 0xC2AC;
uint16_t EEMEM fiforstreg  = 0xCA81;
uint16_t EEMEM synbreg     = 0xCED4;
uint16_t EEMEM drsreg      = 0xC623;
uint16_t EEMEM pllcreg     = 0xCC77;
*/

#define garbage    (void *)0x0000
#define bootMode   (void *)0x0002
#define afcreg     (void *)0x0004
#define txcreg     (void *)0x0006
#define cfsreg     (void *)0x0008
#define rxcreg     (void *)0x000A
#define bbfcreg    (void *)0x000C
#define fiforstreg (void *)0x000E
#define synbreg    (void *)0x0010
#define drsreg     (void *)0x0012
#define pllcreg    (void *)0x0014

void setEEPROMdefaults(void)
{
    eeprom_write_word(bootMode,   MENU);
    eeprom_write_word(afcreg,     0xC4F7);
    eeprom_write_word(txcreg,     0x9810);
    eeprom_write_word(cfsreg,     0xA348);
    eeprom_write_word(rxcreg,     0x94C0);
    eeprom_write_word(bbfcreg,    0xC2AC);
    eeprom_write_word(fiforstreg, 0xCA81);
    eeprom_write_word(synbreg,    0xCED4);
    eeprom_write_word(drsreg,     0xC623);
    eeprom_write_word(pllcreg,    0xCC77);
}

uint8_t getBootState(void)
{
    uint8_t mode = eeprom_read_word(bootMode);
    switch(mode) {
        case SERIAL:
        case SERIAL_ECC:
        case PACKET:
        case PACKET_ECC:
        case MENU:
            return mode;
        default:
            setEEPROMdefaults();
            return MENU;
    }
}

void setBootState(uint8_t newBootMode)
{
//    eeprom_write_word(bootMode, hamming_encode(newBootMode));
    eeprom_write_word(bootMode, newBootMode);
}

void applySavedRegisters(void)
{
    MRF_registerSet(eeprom_read_word(afcreg));
    MRF_registerSet(eeprom_read_word(txcreg));
    MRF_registerSet(eeprom_read_word(cfsreg));
    MRF_registerSet(eeprom_read_word(rxcreg));
    MRF_registerSet(eeprom_read_word(bbfcreg));
    MRF_registerSet(eeprom_read_word(fiforstreg));
    MRF_registerSet(eeprom_read_word(synbreg));
    MRF_registerSet(eeprom_read_word(drsreg));
    MRF_registerSet(eeprom_read_word(pllcreg));
}

const uint8_t afcregString[]     PROGMEM = "\n\r0) AFCREG:     ";
const uint8_t txcregString[]     PROGMEM = "\n\r1) TXCREG:     ";
const uint8_t cfsregString[]     PROGMEM = "\n\r2) CFSREG:     ";
const uint8_t rxcregString[]     PROGMEM = "\n\r3) RXCREG:     ";
const uint8_t bbfcregString[]    PROGMEM = "\n\r4) BBFCREG:    ";
const uint8_t fiforstregString[] PROGMEM = "\n\r5) FIFORSTREG: ";
const uint8_t synbregString[]    PROGMEM = "\n\r6) SYNBREG:    ";
const uint8_t drsregString[]     PROGMEM = "\n\r7) DRSREG:     ";
const uint8_t pllcregString[]    PROGMEM = "\n\r8) PLLCREG:    ";

void printSavedRegisters(void)
{
    sendStringP(afcregString);
    print_hex(eeprom_read_word(afcreg));
    sendStringP(txcregString);
    print_hex(eeprom_read_word(txcreg));
    sendStringP(cfsregString);
    print_hex(eeprom_read_word(cfsreg));
    sendStringP(rxcregString);
    print_hex(eeprom_read_word(rxcreg));
    sendStringP(bbfcregString);
    print_hex(eeprom_read_word(bbfcreg));
    sendStringP(fiforstregString);
    print_hex(eeprom_read_word(fiforstreg));
    sendStringP(synbregString);
    print_hex(eeprom_read_word(synbreg));
    sendStringP(drsregString);
    print_hex(eeprom_read_word(drsreg));
    sendStringP(pllcregString);
    print_hex(eeprom_read_word(pllcreg));
    CDC_Device_Flush(&CDC_interface);
}

void setRegisterValue(uint8_t index, uint16_t value)
{
    switch (index) {
        case 0:
            eeprom_write_word(afcreg, value);
            MRF_registerSet(value);
            break;
        case 1:
            eeprom_write_word(txcreg, value);
            MRF_registerSet(value);
            break;
        case 2:
            eeprom_write_word(cfsreg, value);
            MRF_registerSet(value);
            break;
        case 3:
            // RXCREG bit FINTDIO must be set
            eeprom_write_word(rxcreg, value | MRF_FINTDIO);
            MRF_registerSet(value | MRF_FINTDIO);
            break;
        case 4:
            eeprom_write_word(bbfcreg, value);
            MRF_registerSet(value);
            break;
        case 5:
            eeprom_write_word(fiforstreg, value);
            MRF_registerSet(value);
            break;
        case 6:
            eeprom_write_word(synbreg, value);
            MRF_registerSet(value);
            break;
        case 7:
            eeprom_write_word(drsreg, value);
            MRF_registerSet(value);
            break;
        case 8:
            eeprom_write_word(pllcreg, value);
            MRF_registerSet(value);
            break;
        default:
            return;
    }
    
    // The transciever should be reset after register setting
    MRF_reset();
}
