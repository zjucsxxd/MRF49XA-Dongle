/*
 *  MRF49XA.c
 *  MRF49XA
 *
 *  Created by William Dillon on 11/1/10.
 *  Copyright 2010 Oregon State University. All rights reserved.
 *
 */

#define MRF49XA_C
#include "MRF49XA.h"
#include "spi.h"

#include <util/delay.h>

// Bit position:   7  6  5  4  3  2  1  0
// Normal modes:                 <X  X  X>
// Testing modes: <X  X  X>                   

// Normal modes
#define MRF_IDLE			0x00	// Listening for packets, nothing yet
#define MRF_TRANSMIT_PACKET 0x01	// Actively transmitting a packet
#define MRF_RECEIVE_PACKET  0x02	// Actively receiving a packet

// Testing modes
#define MRF_RECEIVE_ALL		0x20	// Not yet implemented
#define MRF_TRANSMIT_ZERO	0x40	// Transmits all '0'
#define MRF_TRANSMIT_ONE	0x80	// Transmits all '1'
#define MRF_TRANSMIT_ALT	0xC0	// Transmits alternating '01'

#define MRF_TX_TEST_MASK	0xC0	// Singles out spectrum test modes

static volatile uint8_t mrf_state;	// Defaults to idle
static volatile uint8_t mrf_alive;	// Set to '1' by ISR

// There are 2 Rx_Packet_t instances, one for reading off the air
// and one for processing back in main. (double buffering)
MRF_packet_t Rx_Packet_a;
MRF_packet_t Rx_Packet_b;

// The hasPacket flag means that the finished_packet variable contains
// a fresh data packet.  The receiving_packet always contains space for
// received data.  If finished_packet isn't read before the a second packet
// comes
volatile uint8_t	hasPacket;		// Initialized to 0 by default
volatile MRF_packet_t *finished_packet;
volatile MRF_packet_t *receiving_packet;

volatile uint8_t	transmit_buffer[MRF_TX_PACKET_LEN];

volatile uint16_t	mrf_status;

#define RegisterSet(setting) spi_write16(setting, &MRF_CS_PORTx, MRF_CS_BIT)

uint16_t MRF_statusRead(void)
{
	spi_write16(0x0000, &MRF_CS_PORTx, MRF_CS_BIT);
	return spi_read16();
}

static inline uint8_t MRF_fifo_read(void)
{
	RegisterSet(MRF_RXFIFOREG);
	return spi_read8();
}

void MRF_reset(void)
{
	RegisterSet(MRF_PMCREG);
	RegisterSet(MRF_FIFOSTREG_SET);
	RegisterSet(MRF_GENCREG_SET);
	RegisterSet(MRF_GENCREG_SET | MRF_FIFOEN);
	RegisterSet(MRF_FIFOSTREG_SET | MRF_FSCF);
	RegisterSet(MRF_PMCREG | MRF_RXCEN);	

    mrf_status = MRF_IDLE;
}

volatile uint8_t counter;
ISR(MRF_IRO_VECTOR, ISR_BLOCK)
{
	mrf_alive = 1;
	
	uint8_t bl;
	
	// Set the CS pin for the MRF low
	MRF_CS_PORTx &= ~(1 << MRF_CS_BIT);
	
	// Wait for the synchronizer
	_delay_us(1);
	
	// Perform operations
	if (bit_is_set(SPI_MISO_PINx, SPI_MISO_BIT)) {
		
		switch (mrf_state) {
			case MRF_IDLE:
				bl = MRF_fifo_read();
				
                // The first byte is the packet length
                // the maximum length is 128
				if (bl > 128) {
					mrf_state = MRF_RECEIVE_PACKET;
					receiving_packet->payloadSize = bl;
                    // We've received 1 byte
					counter = 1;
				}
                
                // The length doesn't make sense, so reset
                else {
					PORTC &= ~(1 << 4);
					counter = 0;
					MRF_reset();
					return;
				}
                
				break;

			case MRF_TRANSMIT_PACKET:
				// Transmit the contents of the packet
				// (including preamble, sync, length, and dummy byte)
				RegisterSet(MRF_TXBREG | transmit_buffer[counter++]);
				
				// Derivation of the +4:
				// Preamble:      1 + (before packet)
				// Sync word:     2 = (before packet)
				// Total:         4

				// Test for packet finish
                if (counter > (transmit_buffer[3] + 4 +
                               MRF_PACKET_OVERHEAD)) {

					// Disable transmitter, enable receiver
					RegisterSet(MRF_PMCREG | MRF_RXCEN);
					RegisterSet(MRF_GENCREG_SET | MRF_FIFOEN);
					RegisterSet(MRF_FIFOSTREG_SET);
					RegisterSet(MRF_FIFOSTREG_SET | MRF_FSCF);
					
					// Return the state
					mrf_state = MRF_IDLE;
					counter = 0;
				}
				
				break;
								
			case MRF_RECEIVE_PACKET:	// We've received at least the size
                bl = MRF_fifo_read();

                // Convert the packet to a linear string of bytes
                char *packet_bytes = (char *)receiving_packet;
                
                packet_bytes[counter++] = bl;
                
				// End of packet?
				if (counter > receiving_packet->payloadSize + MRF_PACKET_OVERHEAD) {

					// Reset the FIFO
					RegisterSet(MRF_FIFOSTREG_SET);
					RegisterSet(MRF_FIFOSTREG_SET | MRF_FSCF);
					
					// Swap packet structures
					finished_packet = receiving_packet;
					hasPacket = 1;
					if (receiving_packet == &Rx_Packet_a) {
						receiving_packet =  &Rx_Packet_b;
					} else {
						receiving_packet =  &Rx_Packet_a;
					}
					
					// Restore state
					mrf_state = MRF_IDLE;
					counter = 0;
					receiving_packet->payloadSize = 0;
				}			
				
				break;

			case MRF_TRANSMIT_ZERO:
				RegisterSet(MRF_TXBREG | 0x0000);
				break;
				
			case MRF_TRANSMIT_ONE:
				RegisterSet(MRF_TXBREG | 0x00FF);
				break;
				
			case MRF_TRANSMIT_ALT:
				RegisterSet(MRF_TXBREG | 0x00AA);
				break;
				
			default:
				break;
		}
	}

	// There was no FIFO flag, just leave.
	else {
		MRF_CS_PORTx |=  (1 << MRF_CS_BIT);
		return;
	}	
}

void MRF_init(void)
{
	// Enable the IRO pin as input w/o pullup
	MRF_IRO_PORTx &= ~(1 << MRF_IRO_BIT);
	MRF_IRO_DDRx  &= ~(1 << MRF_IRO_BIT);
	
	// Enable the CS line as output with high value
	MRF_CS_PORTx  |=  (1 << MRF_CS_BIT);
	MRF_CS_DDRx   |=  (1 << MRF_CS_BIT);
	
	// Enable the FSEL as output with high value (default, low for receive)
	MRF_FSEL_PORTx |= (1 << MRF_FSEL_BIT);
	MRF_FSEL_DDRx |=  (1 << MRF_FSEL_BIT);
	
	// Enable the External interrupt for the IRO pin (falling edge)
    MRF_INT_SETUP();
	
	// configuring the MRF49XA radio
	RegisterSet(MRF_FIFOSTREG_SET);	// Set 8 bit FIFO interrupt count
	RegisterSet(MRF_FIFOSTREG_SET | MRF_FSCF);	// Enable sync. latch
	RegisterSet(MRF_GENCREG_SET);	// From the header: 434mhz, 10pF
    RegisterSet(MRF_AFCCREG_SET);
    RegisterSet(MRF_CFSREG_SET);
    RegisterSet(MRF_DRSREG_SET);			// Approx. 9600 baud
    RegisterSet(MRF_PMCREG | MRF_CLKODIS);	// Shutdown everything
    RegisterSet(MRF_RXCREG_SET);
    RegisterSet(MRF_TXCREG_SET);
	RegisterSet(MRF_BBFCREG | MRF_ACRLC | (4 & MRF_DQTI_MASK));

    // antenna tuning on startup
    RegisterSet(MRF_PMCREG | MRF_CLKODIS | MRF_TXCEN); // turn on the transmitter
    _delay_ms(5);                            // wait for oscillator to stablize
	// end of antenna tuning
	
	// turn off transmitter, turn on receiver
    RegisterSet(MRF_PMCREG | MRF_CLKODIS | MRF_RXCEN);
    RegisterSet(MRF_GENCREG_SET | MRF_FIFOEN);
	RegisterSet(MRF_FIFOSTREG_SET);
	RegisterSet(MRF_FIFOSTREG_SET | MRF_FSCF);
	
	// Setup the packet pointers
	receiving_packet = &Rx_Packet_a;
	
	// Dummy read of status registers to clear Power on reset flag
	mrf_status = MRF_statusRead();
	
	// Enable interrupt last, just in case they're already globally enabled
	MRF_INT_MASK();
}

// Testing functions
void MRF_transmit_zero()
{
	// If we're already in a "dumb transmit" state, just change the value
	if (mrf_state & MRF_TX_TEST_MASK) {
		mrf_state = MRF_TRANSMIT_ZERO;
		return;
	}
	
	mrf_state = MRF_TRANSMIT_ZERO;

	// Enable the TX Register
	RegisterSet(MRF_GENCREG_SET | MRF_TXDEN);
	
	// The transmit register is filled with 0xAAAA, we want it to be zeros
	RegisterSet(MRF_TXBREG | 0x0000);
	
	// Enable the transmitter
	RegisterSet(MRF_PMCREG | MRF_CLKODIS | MRF_TXCEN);
	
	// Upon completion of a byte !IRO should toggle
	return;	
}

void MRF_transmit_one()
{
	if (mrf_state & MRF_TX_TEST_MASK) {
		mrf_state = MRF_TRANSMIT_ONE;
		return;
	}
	
	mrf_state = MRF_TRANSMIT_ONE;
	
	// Enable the TX Register
	RegisterSet(MRF_GENCREG_SET | MRF_TXDEN);
	
	// The transmit register is filled with 0xAAAA, we want it to be ones
	RegisterSet(MRF_TXBREG | 0x00FF);
	
	// Enable the transmitter
	RegisterSet(MRF_PMCREG | MRF_CLKODIS | MRF_TXCEN);
	
	// Upon completion of a byte !IRO should toggle
	return;	
}

void MRF_transmit_alternating()
{
	if (mrf_state & MRF_TX_TEST_MASK) {
		mrf_state = MRF_TRANSMIT_ALT;
		return;
	}
	
	mrf_state = MRF_TRANSMIT_ALT;
	
	// Enable the TX Register
	RegisterSet(MRF_GENCREG_SET | MRF_TXDEN);
	
	// The transmit register is filled with 0xAAAA, we can leave it alone
	
	// Enable the transmitter
	RegisterSet(MRF_PMCREG | MRF_CLKODIS | MRF_TXCEN);
	
	// Upon completion of a byte !IRO should toggle
	return;	
}

MRF_packet_t* MRF_receive_packet()
{
	if (hasPacket) {
		hasPacket = 0;
		return finished_packet;
	} else {
		return 0;
	}
}

uint8_t MRF_is_idle(void)
{
	if (mrf_state == MRF_IDLE) {
		return 1;
	} else {
		return 0;
	}
}

// This function checks the alive flag in the ISR.
// If for whatever reason the ISR doesn't fire since the last
// time checked return 0, unless the module is idle.
// Otherwise, if the ISR has run since last time, send 1.
uint8_t MRF_is_alive(void)
{
	if (mrf_alive) {
		mrf_alive = 0;
		return 1;
	} else {
		if (mrf_state == MRF_IDLE) {
			return 1;
		}
		return 0;
	}
}

void MRF_transmit_packet(MRF_packet_t *packet)
{
	uint8_t	i;
	uint8_t wait = 1;

	// We can check, without synchronization
	// (because it doesn't change in the ISR)
	// Whether we're in a testing mode.  If so, simply return.
	// This is important, so we don't hard lock.
	if (mrf_state & MRF_TX_TEST_MASK) {
		return;
	}
	
	// Wait for the module to be idle (this is cheezy synchronization)
	do {
		cli();	// Disable interrupts, this is a critical section
		if (mrf_state == MRF_IDLE) {
			mrf_state = MRF_TRANSMIT_PACKET;
			wait = 0;
			sei();			// Atomic operation complete, reenable interrupts
		} else {
			sei();			// Atomic operation complete, reenable interrupts
			_delay_ms(1);	// This should be roughly 1 byte period
		}
	} while (wait);

	// Initialize the constant parts of the transmit buffer
	counter = 0;
	transmit_buffer[0] = 0xAA;
	transmit_buffer[1] = 0x2D;
	transmit_buffer[2] = 0xD4;

    // Copy packet fields
    transmit_buffer[3] = packet->payloadSize;
    transmit_buffer[4] = packet->type;

    // Copy the FEC data
    for (i = 0; i < 32; i++) {
        transmit_buffer[5+i] = packet->FEC[i];
    }
        
	// Copy the packet contents into the buffer
	for (i = 0; i < packet->payloadSize; i++) {
		transmit_buffer[5 + 32 + i] = packet->payload[i];
	}

	RegisterSet(MRF_PMCREG);					// Turn everything off
	RegisterSet(MRF_GENCREG_SET | MRF_TXDEN);	// Enable TX FIFO
	// Reset value of TX FIFO is 0xAAAA
	
	RegisterSet(MRF_PMCREG | MRF_TXCEN);		// Begin transmitting
	// Everything else is handled in the ISR
}
