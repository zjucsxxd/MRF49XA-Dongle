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

// The packet structure is now a mostly blank slate.  The size feild is
// only includes the payload, not the size and type.  The type feild
// contains a hint for the internal structure of the packet.
//
// If the packet structure is changed, the payload size field must be first
// and must not be renamed.  Mark the constant number of bytes that aren't
// included in the payloadSize field in the MRF_PACKET_OVERHEAD define.
//
// These defined may be used to access the maximum payload length in the app.
#define MRF_PAYLOAD_LEN     96

typedef struct {
    char  payloadSize;   // Total size of the payload
    char  type;         // for now, set to 0xBD
    char  payload[MRF_PAYLOAD_LEN];
} MRF_packet_t;

// These defines are used internally to the library, they include 
// the maximum packet size for basic sanity checking, and for internal buffers
#define MRF_PACKET_LEN      sizeof(MRF_packet_t)
// Space for preamble, sync, and dummy
#define MRF_TX_PACKET_LEN	MRF_PACKET_LEN + 4
// Packet overhead (length)
#define MRF_PACKET_OVERHEAD 2

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
void MRF_reset(void);

