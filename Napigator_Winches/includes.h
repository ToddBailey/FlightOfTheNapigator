// Include files:
//==================
// AVR specific stuff:

//#include <ctype.h>
//#include <math.h>
//#include <stdint.h>
//#include <stdlib.h>
#include <stdio.h>
#include <string.h>			// Needed for memcpy()
#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/io.h>
//#include <avr/pgmspace.h>
//#include <util/delay.h>

// Application specific stuff:
//------------------------------

#include "defines.h"
#include "globals.h"
#include "eeprom.h"
#include "softclock.h"
#include "napFlyer.h"
#include "irComm.h"
