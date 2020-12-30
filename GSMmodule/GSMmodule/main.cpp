/*
 * GSMmodule.cpp
 *
 * Created: 14-Sep-19 3:55:34 PM
 * Author : Wizard
 */ 
#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#define BAUD 9600
#define BAUDRATE ((F_CPU)/(BAUD*8UL)-1)

unsigned char cmd1[]={"AT"};
unsigned char cmd2[]={"AT+CMGF=1"};
unsigned char cmd3[]={"AT+CMGS="};
unsigned char msg[]={"Fire"};
unsigned char number[]={"+8801748812048"};

void gsm()
{
	//_delay_ms(50);		//omittable??
	PORTB |= 0x01;
	
	// handshake: Send "AT"
	for(int i=0; i<2; i++)
	{
		while((UCSRA & (1<<UDRE)) == 0x00)
		{
			//_delay_ms(50);	// May be omittable
		}
		UDR = cmd1[i];
		_delay_ms(50);
	}
	
	// Carriage return
	while((UCSRA & (1<<UDRE)) == 0x00)
	{
		//_delay_ms(50);	// May be omittable
	}
	UDR = '\r';
	_delay_ms(50);
	
	// text mode: send "AT+CMGF=1"
	for(int i=0; i<9; i++)
	{
		while((UCSRA & (1<<UDRE)) == 0x00)
		{
			//_delay_ms(50);	// May be omittable
		}
		UDR = cmd2[i];
		_delay_ms(50);
	}
	
	// Carriage return
	while((UCSRA & (1<<UDRE)) == 0x00)
	{
		//_delay_ms(50);	// May be omittable
	}
	UDR = '\r';
	_delay_ms(50);
	
	// send mode: send "AT+CMGS="
	for(int i=0; i<8; i++)
	{
		while((UCSRA & (1<<UDRE)) == 0x00)
		{
			//_delay_ms(50);	// May be omittable
		}
		UDR = cmd3[i];
		_delay_ms(50);
	}
	
	while((UCSRA & (1<<UDRE)) == 0x00)
	{
		//_delay_ms(50);
	}
	UDR = '"';
	_delay_ms(50);
	
	// send number
	for(int i=0; i<14; i++)
	{
		while((UCSRA & (1<<UDRE)) == 0x00)
		{
			//_delay_ms(50);
		}
		UDR = number[i];
		_delay_ms(50);
	}
	
	while((UCSRA & (1<<UDRE)) == 0x00)
	{
		//_delay_ms(50);
	}
	UDR = '"';
	_delay_ms(50);
	
	// Carriage return
	while((UCSRA & (1<<UDRE)) == 0x00)
	{
		//_delay_ms(50);	// May be omittable
	}
	UDR = '\r';
	_delay_ms(50);
	
	// send message
	for(int i=0; i<4; i++)
	{
		while((UCSRA & (1<<UDRE)) == 0x00)
		{
			//_delay_ms(50);	// May be omittable
		}
		UDR = msg[i];
		_delay_ms(50);
	}
	
	while((UCSRA & (1<<UDRE)) == 0x00)
	{
		//_delay_ms(50);
	}

	//send
	UDR = (char) 26;
	_delay_ms(50);
	PORTB = 0x00;
	
}

int main(void)
{
	UCSRA = 0x00;
	UCSRB = 0x00;
	UCSRC = 0x00;
	UBRRH = 0x00;
	UBRRL = 0x0C;	// UBRR = 12.02
	
	UCSRA |= (1<<U2X);	// Double processor speed
	UCSRB |= (1<<TXEN) | (1<<RXEN);	// Enable TX and RX pin
	UCSRC |= (1<<URSEL) | (1<<UCSZ1) | (1<<UCSZ0);	// URSEL = Activate UCSRC
													// UCSZ1:UCSZ0 = 8 bit data
	DDRA |= 0xFC;	// PINA0 Input
	DDRB |= 0xFF;
	
	
    while (1) 
    {
		if((PINA) & (1<<PINA0))
		{
			gsm();
		}
				
    }
}

