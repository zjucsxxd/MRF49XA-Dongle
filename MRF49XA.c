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
#include "hamming.h"

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

volatile uint8_t packetCounter;

// There are 2 Rx_Packet_t instances, one for reading off the air
// and one for processing back in main. (double buffering)
MRF_packet_t Rx_Packet_a;
MRF_packet_t Rx_Packet_b;
MRF_packet_t Tx_packet;

// The hasPacket flag means that the finished_packet variable contains
// a fresh data packet.  The receiving_packet always contains space for
// received data.  If finished_packet isn't read before the a second packet
// comes the contents are lost
volatile uint8_t	hasPacket;		// Initialized to 0 by default
volatile MRF_packet_t *finished_packet;
volatile MRF_packet_t *receiving_packet;

volatile uint16_t	mrf_status;

static volatile uint8_t fiforstregUser = MRF_DRSTM;

#define RegisterSet(setting) spi_write16(setting, 0xFFFF, &MRF_CS_PORTx, MRF_CS_BIT)

extern volatile uint8_t mutex;

void MRF_registerSet(uint16_t value)
{
    // We need to detect whether the FIFORSTREG is being set.  There are user
    // flags and core flags in the same register, therefore, we need to save
    // the user parts of it, and bitwise-OR them with the core flags.
    if (value & 0xFF00 == MRF_FIFORSTREG) {
        fiforstregUser = value & (MRF_DRSTM | MRF_SYCHLEN); // Filter-out all but the user fields
        return;
    }
    
    RegisterSet(value);
}

uint16_t MRF_statusRead(void)
{
    spi_write16(0x0000, 0xFFFF, &MRF_CS_PORTx, MRF_CS_BIT);
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
	RegisterSet(MRF_FIFOSTREG_SET | fiforstregUser);
	RegisterSet(MRF_GENCREG_SET);
	RegisterSet(MRF_GENCREG_SET | MRF_FIFOEN);
	RegisterSet(MRF_FIFOSTREG_SET | MRF_FSCF | fiforstregUser);
	RegisterSet(MRF_PMCREG | MRF_RXCEN);	

    mrf_status = MRF_IDLE;
    LED_PORTx &= ~(1 << LED_RX) & ~(1 << LED_TX);
}

static inline void idle_ISR(void)
{
    uint8_t bl = MRF_fifo_read();

    // The first byte is the packet payload length, make sure it's sensical
    if (bl <= MRF_PAYLOAD_LEN && bl > 0) {
        mrf_state  = MRF_RECEIVE_PACKET;
        LED_PORTx |= (1 << LED_RX);

        receiving_packet->payloadSize = bl;
        for (int i = 0; i < bl; i++) {
            receiving_packet->payload[i] = 0;   // Clean the previous payload
        }
        
        // We've received 1 byte
        packetCounter = 1;
    }
    
    // The length doesn't make sense, so reset
    else {
        LED_PORTx &= ~(1 << LED_RX);
        packetCounter = 0;
        MRF_reset();
        return;
    }
}

static inline void xmit_ISR(void)
{
    uint8_t maxPacketCounter = 0;
    
    // ECC payloads are twice as large as advertised
    if (Tx_packet.type == PACKET_TYPE_SERIAL_ECC ||
        Tx_packet.type == PACKET_TYPE_PACKET_ECC) {
        maxPacketCounter = (Tx_packet.payloadSize * 2) + MRF_TX_PACKET_OVERHEAD;
    } else {
        maxPacketCounter = Tx_packet.payloadSize + MRF_TX_PACKET_OVERHEAD;
    }
    
    // Test whether we're done transmitting
    if (packetCounter >= maxPacketCounter) {
        // Disable transmitter, enable receiver
        RegisterSet(MRF_PMCREG | MRF_RXCEN);
        RegisterSet(MRF_GENCREG_SET | MRF_FIFOEN);
        RegisterSet(MRF_FIFOSTREG_SET | fiforstregUser);
        RegisterSet(MRF_FIFOSTREG_SET | fiforstregUser | MRF_FSCF);
        
        // Return the state
        mrf_state = MRF_IDLE;
        LED_PORTx &= ~(1 << LED_TX);
        packetCounter = 0;
    }
    
    switch (packetCounter) {
        case 0:         // First byte is 'AA' for a alternating tone
            RegisterSet(MRF_TXBREG | 0x00AA);
            break;
        case 1:         // First of two synchronization bytes
            RegisterSet(MRF_TXBREG | 0x002D);
            break;
        case 2:         // Second of two synchronization bytes
            RegisterSet(MRF_TXBREG | 0x00D4);
            break;
        case 3:         // Size byte
            RegisterSet(MRF_TXBREG | Tx_packet.payloadSize);
            break;
        case 4:         // Type byte
            RegisterSet(MRF_TXBREG | Tx_packet.type);
            break;
            
        default:        // Payload
            // It matters which mode we're in.
            // If we're in an ECC mode, we transmit hamming-coded
            // high-nibbles on high-packet
            if (Tx_packet.type == PACKET_TYPE_SERIAL_ECC ||
                Tx_packet.type == PACKET_TYPE_PACKET_ECC) {
                
                // Calculate the payload byte we're using (divide by 2)
                uint8_t payloadByte = Tx_packet.payload[(packetCounter - 5) >> 1];

                // If the payload index is odd, we're transmitting the high nibble
                if ((packetCounter - 5) & 0x01) {
                    RegisterSet(MRF_TXBREG | hamming_encode_nibble(payloadByte >> 4));
                }

                // Otherwise, it's the low nibble
                else {
                    RegisterSet(MRF_TXBREG | hamming_encode_nibble(payloadByte & 0x0F));
                }
                
            } else {
                // The 5 is from the preamble, 2 sync bytes, size and type bytes.
                // Later, we'll need to include ECC calculation here.
                RegisterSet(MRF_TXBREG | Tx_packet.payload[packetCounter - 5]);
            }
            
            break;
    }
    
    packetCounter += 1;
}

// If this ISR function is called, we've recieved the payload length and nothing else
static inline void rx_ISR(void)
{
	uint8_t bl = MRF_fifo_read();
    
    if (packetCounter == 1) {
        // We're recieving the type field
        receiving_packet->type = bl;
        packetCounter++;
        return;
    }
    
    // We've got the type field, so we know whether it's ECC, and 2x the size
    uint8_t maxPacketCounter = receiving_packet->payloadSize + MRF_PACKET_OVERHEAD;
    if (receiving_packet->type == PACKET_TYPE_SERIAL_ECC ||
        receiving_packet->type == PACKET_TYPE_PACKET_ECC) {
        maxPacketCounter = (receiving_packet->payloadSize * 2) + MRF_PACKET_OVERHEAD;
        
        // Get the location into the payload field
        uint8_t index = (packetCounter - MRF_PACKET_OVERHEAD) >> 1;
        
        // If the packet counter is odd, we're recieving the high nibble
        // The packet is cleared out beforehand, so we can just or-in the new info
        if ((packetCounter - MRF_PACKET_OVERHEAD) & 0x01) {
            receiving_packet->payload[index] |= hamming_decode_nibble(bl) << 4;
        }
        // Otherwise, we're receiving the low nibble
        else {
            receiving_packet->payload[index] |= hamming_decode_nibble(bl) & 0x0F;
        }
    } else {
        receiving_packet->payload[packetCounter - MRF_PACKET_OVERHEAD] = bl;
    }

    packetCounter++;
    
    // End of packet?
    if (packetCounter >= maxPacketCounter) {
        // Reset the FIFO
        RegisterSet(MRF_FIFOSTREG_SET | fiforstregUser);
        RegisterSet(MRF_FIFOSTREG_SET | fiforstregUser | MRF_FSCF);
        
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
        LED_PORTx &= ~(1 << LED_RX);
        packetCounter = 0;
        receiving_packet->payloadSize = 0;
    }
}

ISR(MRF_IRO_VECTOR, ISR_BLOCK)
{
	// Set the MRF's CS pin low
	MRF_CS_PORTx &= ~(1 << MRF_CS_BIT);

	// This needs to be here to delay for the synchronizer
	mrf_alive = 1;
	
	// the MISO pin marks whether the FIFO needs attention
	if (bit_is_set(SPI_MISO_PINx, SPI_MISO_BIT)) {
		
		switch (mrf_state) {
			case MRF_IDLE:              // Passively receiving
                idle_ISR();
                break;

			case MRF_TRANSMIT_PACKET:   // Actively transmitting
                xmit_ISR();
                break;
								
			case MRF_RECEIVE_PACKET:	// We've received at least the size
				rx_ISR();
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

void MRF_init()
{
    // The Chip Select is the only SPI pin that needs to be set here.
    // The rest are taken care of in the SPI init function.
    
	// Enable the IRO pin as input w/o pullup
	MRF_IRO_PORTx &= ~(1 << MRF_IRO_BIT);
	MRF_IRO_DDRx  &= ~(1 << MRF_IRO_BIT);
	
	// Enable the CS line as output with high value
	MRF_CS_PORTx  |=  (1 << MRF_CS_BIT);
	MRF_CS_DDRx   |=  (1 << MRF_CS_BIT);
	
	// Enable the FSEL as output with high value (default, low for receive)
	MRF_FSEL_PORTx |= (1 << MRF_FSEL_BIT);
	MRF_FSEL_DDRx  |= (1 << MRF_FSEL_BIT);
	
	// Enable the External interrupt for the IRO pin (falling edge)
    MRF_INT_SETUP();
	
	// configuring the MRF49XA radio
	RegisterSet(MRF_FIFOSTREG_SET);             // Set 8 bit FIFO interrupt count
	RegisterSet(MRF_FIFOSTREG_SET | MRF_FSCF);	// Enable sync. latch
	RegisterSet(MRF_GENCREG_SET);               // From the header: 434mhz, 10pF
    RegisterSet(MRF_PMCREG | MRF_CLKODIS);      // Shutdown everything

    RegisterSet(MRF_TXCREG  | MRF_MODBW_30K | MRF_OTXPWR_0);
    RegisterSet(MRF_RXCREG  | MRF_FINTDIO   | MRF_RXBW_67K | MRF_DRSSIT_103db);
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
	
    mrf_state = MRF_IDLE;
    
	// Enable interrupt last, just in case they're already globally enabled
	MRF_INT_MASK();
}

void MRF_set_freq(uint16_t freqb)
{
    // Mask the input value
    freqb = freqb & MRF_FREQB_MASK;
    
    // Make sure it's within the range (do nothing if its not)
    if (freqb < 97 || freqb > 3903) {
        return;
    }
    
    RegisterSet(MRF_CFSREG | freqb);
}

// Testing functions
void MRF_transmit_zero()
{
    // If we're already doing a spectrum test, just mark the new pattern
	if (mrf_state & MRF_TX_TEST_MASK) {
		mrf_state = MRF_TRANSMIT_ZERO;
		return;
	}
	
	mrf_state = MRF_TRANSMIT_ZERO;
    LED_PORTx |= (1 << LED_TX);
    
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
    // If we're already doing a spectrum test, just mark the new pattern
	if (mrf_state & MRF_TX_TEST_MASK) {
		mrf_state = MRF_TRANSMIT_ONE;
		return;
	}
	
	mrf_state = MRF_TRANSMIT_ONE;
    LED_PORTx |= (1 << LED_TX);
	
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
    // If we're already doing a spectrum test, just mark the new pattern
	if (mrf_state & MRF_TX_TEST_MASK) {
		mrf_state = MRF_TRANSMIT_ALT;
		return;
	}
	
	mrf_state = MRF_TRANSMIT_ALT;
    LED_PORTx |= (1 << LED_RX);

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
	// Whether we're in a testing mode.
    // If we are, reset the device and proceed
	if (mrf_state & MRF_TX_TEST_MASK) {
        MRF_reset();
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

    LED_PORTx |= (1 << LED_TX);

	// Initialize the constant parts of the transmit buffer
	packetCounter = 0;

    // Copy the packet
    Tx_packet.payloadSize = packet->payloadSize;
    Tx_packet.type        = packet->type;
    uint8_t *packet_bytes = (uint8_t *)packet;
    for (i = 0; i < packet->payloadSize; i++) {
        Tx_packet.payload[i] = packet->payload[i];
    }

	RegisterSet(MRF_PMCREG);					// Turn everything off
	RegisterSet(MRF_GENCREG_SET | MRF_TXDEN);	// Enable TX FIFO
	// Reset value of TX FIFO is 0xAAAA
	
	RegisterSet(MRF_PMCREG | MRF_TXCEN);		// Begin transmitting
	// Everything else is handled in the ISR
}
