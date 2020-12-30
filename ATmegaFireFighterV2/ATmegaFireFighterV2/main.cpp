/*
 * AtmegaFirefighter.cpp
 *
 * Created: 05-Sep-19 6:55:06 PM
 * Author : Wizard
 */ 

#define F_CPU 1000000UL // CPU clock = 1 MHz
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/***
* DEFINE THIS PINS FOR lcd.h
***/

#define D4 eS_PORTA4
#define D5 eS_PORTA5
#define D6 eS_PORTA6
#define D7 eS_PORTA7
#define RS eS_PORTC6
#define EN eS_PORTC7

#include "lcd.h"

/***
 * SERVO CONSTANTS
 * 
 * f_pwm = f_cpu/(N(1+TOP)), where N is the prescaler and TOP is set by OCR1
 * TOP = f_cpu/(N * f_pwm) - 1
 * Here, TOP = 1 MHz/(1 * 50 Hz) - 1 = 19999
 *
 * [Servo Motor MG995 TowerPro - Motor to move the sensor]
 * 0.15 s / 60 deg = 0.45 s / 180 deg => Wait at least 450 ms for full rotation
 * 1 MHz / 1 = 1 MHz = 1 us per count
 ***/

#define SERVO_SENSOR_TOP		19999
#define SERVO_SENSOR_CENTER		1500
#define SERVO_SENSOR_FULL_LEFT	900
#define SERVO_SENSOR_FULL_RIGHT 2100
#define SERVO_SENSOR_STEP		60

/***
* STATE CONSTANTS
***/
#define STATE_INITIALIZE	0
#define STATE_SWEEP			1
#define STATE_READ_SENSOR	2
#define STATE_FIRE_DETECTED	3
#define STATE_PUMP_ON		4

/***
* DIRECTION CONSTANTS
***/
#define GO_LEFT				-1
#define GO_RIGHT			1
#define DELAY				75

/***
* ADC & LCD MACROS
***/
#define START_ADC_CONVERSION();			ADCSRA|=(1<<ADSC);
#define WAIT_UNTIL_CONVERSION_END();	while(ADCSRA & (1<<ADSC)){}

/***
* FLAME SENSOR CONSTANTS
***/
#define FIRE_THRESHOLD			100
#define FIRE_CONFIRMATION_COUNT	3

/***
* PUMP CONSTANTS
***/
#define PUMP_ON_DELAY		850		

/***
* GLOBAL VARIABLES
***/
volatile int sensor_value;
volatile int state;
volatile int direction;
volatile int init_step;
volatile int fire_state_counter;

void initialize()
{
	DDRA |= 0xFD;	//PORT-A as OUTPUT, except PA1(ADC1 pin)
	DDRB |= 0xFF;	//PORT-B as OUTPUT
	DDRC |= 0xFF;	//PORT-C as OUTPUT of SENSOR
	DDRD |= 0xFF;	//PORT-D as OUTPUT
	
	/***
	* SERVO INITIALIZATION
	*
	* Set COM1A1:COM1A0 = 10
	* to clear OC1A/OC1B on compare match
	* and set OC1A/OC1B at BOTTOM
	*
	* Set WGM13:WGM12:WGM11:WGM10 = 1110
	* to select 'Fast PWM' mode
	* and in this mode TOP value should be set to ICR1
	*
	* Set CS12:CS11:CS10 = 001
	* to set prescaler = 1
	***/
	TCCR1A &= 0;
	TCCR1B &= 0;
	
	TCCR1A |= 1<<WGM11 | 1<<COM1A1;	// Set WGM11=1, COM1A1=1
	TCCR1B |= 1<<WGM13 | 1<<WGM12 | 1<<CS10; // Set WGM13=1, WGM12=1, CS10=1
	ICR1 = SERVO_SENSOR_TOP;
	
	OCR1A = SERVO_SENSOR_CENTER;
	direction = GO_LEFT;	// Initially the servo will start sweeping left
	init_step = 0;			// Start step counting from 0
	
	/***
	* LCD & ADC INITIALIZATION
	***/
	ADMUX  = 0b01000001;	// (bit 7,6)		REFS[1:0]=01	-> AVCC as reference
							// (bit 4,3,2,1,0)	MUX[4:3:2:1:0]=00001 -> Input source = ADC1(PA1/pin39)
	ADCSRA = 0b10000001;	// (bit 7)			ADEN=1 -> enable ADC unit
							// (bit 2,1,0)		ADPS[2:1:0]=001 -> Set division factor = 2
	Lcd4_Init();			// Initialize lcd for 4 bit mode
	
	// Initialize Fire state counter
	fire_state_counter = 0;
	
	// Pump is off initially
	PORTC = 0x00;
}

void get_sensor_data()
{
	START_ADC_CONVERSION();
	WAIT_UNTIL_CONVERSION_END();
	
	uint16_t res = 0;
	res = ADCL;
	uint16_t temp;
	temp = ADCH;
	temp = (temp << 8);
	res |= temp;
	
	sensor_value = (int) res;
}

void print_sensor_data()
{
	Lcd4_Clear();	// Clear lcd screen before printing
	Lcd4_Set_Cursor(1,1);
	Lcd4_Write_String("Value:");
	Lcd4_Set_Cursor(2,1);
	
	char str[5];
	str[4] = '\0';
	str[3] = (char)(sensor_value % 10 + 48);
	str[2] = (char)((sensor_value % 100) / 10 + 48);
	str[1] = (char)((sensor_value % 1000) / 100 + 48);
	str[0] = (char)((sensor_value % 10000) / 1000 + 48);
	
	Lcd4_Write_String(str);
}

void print_fire_alarm()
{
	Lcd4_Clear();					//
	Lcd4_Set_Cursor(1,1);			// Print warning on LCD display
	Lcd4_Write_String("Fire!!!");	//
	// debug purpose
	char count_str[2];
	count_str[1] = '\0';
	count_str[0] = (char)(fire_state_counter%10 + 48);
	Lcd4_Set_Cursor(1,12);
	Lcd4_Write_String(count_str);
	
	Lcd4_Set_Cursor(2,1);
	Lcd4_Write_String("Value:");
	Lcd4_Set_Cursor(2,8);
	char str[5];
	str[4] = '\0';
	str[3] = (char)(sensor_value % 10 + 48);
	str[2] = (char)((sensor_value % 100) / 10 + 48);
	str[1] = (char)((sensor_value % 1000) / 100 + 48);
	str[0] = (char)((sensor_value % 10000) / 1000 + 48);
	Lcd4_Write_String(str);
	
	_delay_ms(DELAY);				// delay DELAY ms between displaying "Fire!!!" and (for example)"Value: 13"
}

void print_pump_msg()
{
	Lcd4_Clear();
	Lcd4_Set_Cursor(1,1);
	Lcd4_Write_String("Pump ON");
	Lcd4_Set_Cursor(2,1);
	Lcd4_Write_String("Throwing Water");
	_delay_ms(DELAY);
}

void throw_water()
{
	PORTC |= (1<<PC1);	// High signal at PC1, clockwise rotation
	char watering_step = 4;
	int inc = (SERVO_SENSOR_FULL_RIGHT - SERVO_SENSOR_FULL_LEFT) / SERVO_SENSOR_STEP;
	if(direction == GO_RIGHT)
	{
		int i = init_step-4;
		while(i<init_step+watering_step)
		{
			OCR1A = SERVO_SENSOR_FULL_LEFT + i*inc;
			_delay_ms(PUMP_ON_DELAY);	// Turn on motor for PUMP_ON_DELAY ms
			i++;
		}
		while(i>=init_step-4)
		{
			OCR1A = SERVO_SENSOR_FULL_LEFT + i*inc;
			_delay_ms(PUMP_ON_DELAY);
			i--;
		}
	}
	else if(direction == GO_LEFT)
	{
		int i = init_step-4;
		while(i<init_step+watering_step)
		{
			OCR1A = SERVO_SENSOR_FULL_RIGHT - i*inc;
			_delay_ms(PUMP_ON_DELAY);	// Turn on motor for PUMP_ON_DELAY ms
			i++;
		}
		while(i>=init_step-4)
		{
			OCR1A = SERVO_SENSOR_FULL_RIGHT - i*inc;
			_delay_ms(PUMP_ON_DELAY);
			i--;
		}
	}
	PORTC &= ~(1<<PC1); // Low signal at PC1, stop motor
}

int sweep_right()
{
	int inc = (SERVO_SENSOR_FULL_RIGHT - SERVO_SENSOR_FULL_LEFT) / SERVO_SENSOR_STEP;
	for(int i=init_step; i<SERVO_SENSOR_STEP; i++)
	{
		// Generate PWM to start Servo motor
		OCR1A = SERVO_SENSOR_FULL_LEFT + i*inc;
		// Read data from IR sensor and print to LCD
		get_sensor_data();
		print_sensor_data();
		if(sensor_value < FIRE_THRESHOLD)
		{
			// Fire detected
			init_step = i;					// store i, so that servo can start from the position it stopped last.
			return STATE_FIRE_DETECTED;		// Do not give further PWM signal to motor
		}
		_delay_ms(DELAY);
	}
	// Servo completed all steps. So set init_step to 0
	init_step = 0;
	// Servo is now in right most position. So servo should start sweeping left.
	direction = GO_LEFT;

	return STATE_SWEEP;		// Keep sweeping
}

int sweep_left()
{
	int inc = (SERVO_SENSOR_FULL_RIGHT - SERVO_SENSOR_FULL_LEFT) / SERVO_SENSOR_STEP;
	for(int i=init_step; i<SERVO_SENSOR_STEP; i++)
	{
		// Generate PWM to start Servo motor
		OCR1A = SERVO_SENSOR_FULL_RIGHT - i*inc;
		// Read data from IR sensor and print to LCD
		get_sensor_data();
		print_sensor_data();
		if(sensor_value < FIRE_THRESHOLD)
		{
			// Fire detected
			init_step = i;					// store i, so that servo can start from the position it stopped last.
			return STATE_FIRE_DETECTED;		// Do not give further PWM signal to motor
		}
		_delay_ms(DELAY);
	}
	// Servo completed all steps. So set init_step to 0
	init_step = 0;
	// Servo is now in left most position. So servo should start sweeping right.
	direction = GO_RIGHT;
	
	return STATE_SWEEP;		// Keep sweeping
}

int main(void)
{
	state = STATE_INITIALIZE;
    //initialize();
	
    while (1) 
    {	
		//get_and_print_sensor_data();
		//_delay_ms(500);
		switch (state)
		{
			case STATE_INITIALIZE:
				initialize();
				state = STATE_SWEEP;
				break;
			
			case STATE_SWEEP:
				// if a fire is detected sweep_left & sweep_right both function return STATE_FIRE_DETECTED
				// else they both return STATE_SWEEP
				if(direction==GO_LEFT)
				{
					state = sweep_left();
				}
				else if(direction==GO_RIGHT) 
				{
					state = sweep_right();
				}
				break;
				
			case STATE_FIRE_DETECTED:
				get_sensor_data();
				print_fire_alarm();
				if(sensor_value > FIRE_THRESHOLD)
				{
					// Fire extinguished
					if(fire_state_counter >= -2) fire_state_counter--;	// if fire is already sensed wait a bit for confirmation before re-initializing
					else
					{
						fire_state_counter = 0;	// re-initialize
						state = STATE_SWEEP;	// Start sweeping again
					}
				}
				else fire_state_counter++;
				if(fire_state_counter > FIRE_CONFIRMATION_COUNT)
				{
					// fire detection confirmed
					// Signal sent to Atmega of GSM
					PORTA |= (1<<PA0);
					//_delay_ms(50);
					//PORTA &= ~(1<<PA0);
					
					// start pump to throw water 
					state = STATE_PUMP_ON;
				}
				
				break;
			
			case STATE_PUMP_ON:
				print_pump_msg();
				throw_water();
				get_sensor_data();
				print_sensor_data();
				if(sensor_value > FIRE_THRESHOLD)
				{
					PORTA &= ~(1<<PA0);
					// Fire extinguished
					fire_state_counter = 0;
					state = STATE_SWEEP;
				}
				break;
		}
    }
}

