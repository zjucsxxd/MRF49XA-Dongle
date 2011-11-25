/*
 *  MRF49XA.h
 *  MRF49XA
 *
 *  Created by William Dillon on 11/1/10.
 *  Copyright 2010 Oregon State University. All rights reserved.
 *
 *  Adapted from Microchip MRF49XA sample code (for register states)
 */

#include "MRF49XA_definitions.h"
#include "hardware.h"

/*******************************************************************************
 * This section of the header file includes the interface used by the user
 * application.  This includes an initialization routine, and functions to set
 * desired center frequency, baud rate, deviation, etc.
 *
 * In addition to initialization an configuration functions, functions are
 * provided for sending a packet, as well as receiving one.
 *
 ******************************************************************************/
void MRF_init(void);

uint8_t MRF_is_idle(void);
uint8_t MRF_is_alive(void);
uint16_t MRF_statusRead(void);

// There is a technical reason for the size and makup of the packet
// The Reed-solomon code we're using is a fixed (255,223) Reed Solomon code
// with up to 8-bit codes.  The (255,223) means that the total size of the
// code word is 255 bytes and there are 223 bytes of useful data,
// which comes out to 32 bytes of parity.
// 
// There isn't much use to calculating the parity of the type and size of the
// packet, as they need to be available before the parity is collected.
//
// The payload section may be smaller than the defined size.  It is possible
// to make a custom struct that is smaller, or you may malloc an array that is
// 34 bytes + the payload size.
typedef struct {
    char  payloadSize;  // size of the payload field
    char  type;         // May be used to mark different types of traffic
    char  FEC[32];      // Reed Solomon FEC for the packet
    char  payload[223]; // actual payload
} MRF_packet_t;

// These defines are used internally to the library, they include 
// the maximum packet size for basic sanity checking, and for internal buffers
#define MRF_PAYLOAD_LEN 256 // the maximum payload size
// Space for preamble, sync, and dummy
#define MRF_TX_PACKET_LEN	MRF_PAYLOAD_LEN + 4

// Packet based functions
void MRF_transmit_packet(MRF_packet_t *packet);
MRF_packet_t* MRF_receive_packet(void);

void MRF_set_baud(uint16_t baud);	// Sets the baud rate in kbps

// Testing functions
void MRF_transmit_zero(void);
void MRF_transmit_one(void);
void MRF_transmit_alternating(void);
void MRF_packet_reflect(void);
void MRF_packet_generator(void);
void MRF_receive(void);

