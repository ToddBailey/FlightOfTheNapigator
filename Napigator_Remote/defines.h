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
	TIMER_DEBOUNCE=0,
	TIMER_MOTOR_RUN,
	TIMER_IR_OUTPUT,
	TIMER_INACTIVITY,
	NUM_TIMERS,
};



//-----------------------------------------------------------------------
// Switch Inputs:

#define	I_JOG_FRONT_UP		(PC0)
#define	I_JOG_FRONT_DOWN	(PC1)
#define	I_JOG_REAR_UP		(PC2)		
#define	I_JOG_REAR_DOWN		(PC3)
#define	I_FULL_UP			(PC4)		
#define	I_FULL_DOWN			(PC5)

// Masks:
#define		Im_JOG_FRONT_UP		(1<<I_JOG_FRONT_UP)
#define		Im_JOG_FRONT_DOWN	(1<<I_JOG_FRONT_DOWN)
#define		Im_JOG_REAR_UP		(1<<I_JOG_REAR_UP)
#define		Im_JOG_REAR_DOWN	(1<<I_JOG_REAR_DOWN)	
#define		Im_FULL_UP			(1<<I_FULL_UP)
#define		Im_FULL_DOWN		(1<<I_FULL_DOWN)		
