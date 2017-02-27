// Global Definitions and Typedefs go here, but not variable declarations:

#define F_CPU	8000000UL					// System clock frequency, usually the crystal or internal oscillator
#define	SECOND	((F_CPU/256)/128)			// Softclock ticks in a second.  Dependent on the system clock frequency and hardware timer resolution/prescaler

// Make a C version of "bool"
//----------------------------
typedef unsigned char bool;
#define		false			(0)
#define		true			(!(false))

typedef void				// Creates a datatype, here a void function called STATE_FUNC().
	STATE_FUNC(void);

enum	// A list of the sub states.  Propers to Todd Squires for this state machine stuff.
	{
		SS_0=0,
		SS_1,
		SS_2,
		SS_3,
		SS_4,
		SS_5,
		SS_6,
		SS_7,
		SS_8,
	};		

#define		MACRO_DoTenNops	asm volatile("nop"::); asm volatile("nop"::); asm volatile("nop"::); asm volatile("nop"::); asm volatile("nop"::); asm volatile("nop"::); asm volatile("nop"::); asm volatile("nop"::); asm volatile("nop"::); asm volatile("nop"::);


// Timers:
//-----------------------------------------------------------------------
// Software timer variables:
enum								// Add more timers here if you need them, but don't get greedy.
{
	TIMER_MOTOR=0,
	NUM_TIMERS,
};

