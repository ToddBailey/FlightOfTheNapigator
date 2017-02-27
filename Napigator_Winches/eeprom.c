// EEPROM Read and Write Functions
// TMB
// Thu Nov 13 23:55:06 CST 2008

// Taken from the ATMEGA164p datasheet more or less.
// Remember that these don't check for flash programming!
// NOTE:  Accepted lore is not to use eeprom address 0 since it's the most likely one to get buggered in a crash.

// NOTE:
// Thu Nov 21 23:35:28 EST 2013
// Smaller parts (like the Atmega48p) must address eeprom memory with a byte, so there's only EEARL and not EEAR

#include "includes.h"

void EepromWrite(unsigned char theAddress, unsigned char theData)	// For the Atmega48p (There's no EEARH)
//void EepromWrite(unsigned int theAddress, unsigned char theData)
{
	unsigned char 
		sreg;

	while(EECR&(1<<EEPE))	// Spin until EEPROM is ready.
		;

	sreg=SREG;	// Keep operations atomic (interrupts can mess up eeprom access)
	cli();		// Stop interrupts.

//	EEAR=theAddress;
	EEARL=theAddress;
	EEDR=theData;
	EECR|=(1<<EEMPE);	// Start the write.
	EECR|=(1<<EEPE);	// Second start-write command.

	SREG=sreg;			// Restore interrupts.
//	EEAR=0;				// Point the address away from the registers we care about.
	EEARL=0;				// Point the address away from the registers we care about.
}

unsigned char EepromRead(unsigned char theAddress)
{
	unsigned char 
		sreg;

	while(EECR&(1<<EEPE))	// Spin until EEPROM is ready.
		;

	sreg=SREG;	// Keep operations atomic (interrupts can mess up eeprom access)
	cli();		// Stop interrupts.

//	EEAR=theAddress;
	EEARL=theAddress;

	EECR|=(1<<EERE);	// Start reading.

	SREG=sreg;			// Restore interrupts.
//	EEAR=0;				// Point the address away from the registers we care about.
	EEARL=0;
	
	return(EEDR);		// Pass the data back from eeprom.
}
