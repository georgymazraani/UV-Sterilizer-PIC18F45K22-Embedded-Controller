#ifndef LCDNEW_H
#define	LCDNEW_H
#ifndef 	_LCD_H
#define     	_LCD_H

#include	<string.h>
#include	<delays.h>
#include	<LCDdefs.h>
						
void SendCmd(char);
void SendNibble(char);

#define     E       PORTAbits.RA5
#define     RS      PORTAbits.RA4

void InitLCD(void)                                                  // 4-bit mode, <RB3:RB0> = data
{
    char LCDstr[] = {0x33, 0x32, 0x28, 0x0C, 0x01, 0x06, '\0'};
    unsigned char i = 0;

    ANSELA = 0x00; TRISA &= 0b11000000;                             // RB5 ... RB0 are output pins
    while (LCDstr[i] != '\0')
        SendCmd(LCDstr[i++]);
}

void SendCmd(char cmd)                                              // send a command to the LCD
{
    RS = 0; E = 0;                                                  // set LCD in command mode
    SendNibble(cmd >> 4);                                           // send upper nibble of ASCII data
    SendNibble(cmd);                                                // send lower nibble of ASCII data
}

void SendChar(char ch)                                              // send a character to the LCD
{
    RS = 1; E = 0;                                                  // set LCD in data mode
    SendNibble(ch >> 4);                                            // send upper nibble of ASCII data
    SendNibble(ch);                                                 // send lower nibble of ASCII data
}

void SendNibble(char ch)
{
    PORTA = (PORTA & 0xF0) | (ch & 0x0F);                           // send data to LCD W/O modifying PORTB<7:4>
    E = 1;                                                          // toggle E: +ve edge latches RS & R_W
    Delay10TCYx(100);                                               // 100 cycles delay
    E = 0;                                                          // -ve edge latches data/command
}

// display string in ROM at StartPos
void DispRomStr(char StartPos, ROM Str)		
{
    SendCmd(StartPos);
    while (*Str != '\0')
        SendChar(*Str++);
}

// display string in RAM at StartPos
void DispRamStr(char StartPos, char *Str)	
{																
    SendCmd(StartPos);
    while (*Str != '\0')
	SendChar(*Str++);
}

// Display string of specified length at StartPos.
void DispVarStr(char *Str, char StartPos, unsigned char NumOfChars)	
{
    unsigned char i;

    SendCmd(StartPos);
    for (i = 0; i < NumOfChars; i++)
        SendChar(*Str++);
}				

// Display a number of blank characters at StartPos.
void DispBlanks(char StartPos, unsigned char NumOfChars)	
{
    unsigned char i;

    SendCmd(StartPos);
    for (i = 0; i < NumOfChars; i++)
        SendChar(' ');                                              // send a blank character
}

// remove conflict with Hyperterm.h
#ifndef     _HYPERTERM_H

// Convert 8-bit N to ASCII characters and store in array a (starting with MSD)
void Bin2Asc(unsigned char N, char *a)                              // max of 425 cycles
{
    a[2] = (N % 10) + '0';                                          // least significant ASCII digit
    N = N / 10;
    a[1] = (N % 10) + '0';                                          // middle significant ASCII digit
    a[0] = (N / 10) + '0';                                          // most significant ASCII digit
}

// Convert 16-bit N to ASCII characters and store in array a (starting with MSD)
void Bin2AscE(unsigned int N, char *a)                              // max of 2212 cycles
{
    unsigned char i;
		
    for (i = 0; i < 4; i++)
    {
        a[4-i] = (N % 10) + '0';                                    // rem of division by 10 in ASCII
	N = N / 10;
    }
    a[0] = N + '0';
}

// Convert BCD array of length Len to ASCII.
void Bcd2Asc(char *a, unsigned char Len)			
{
    unsigned char i;

    for (i = 0; i < Len; i++)
        a[i] += '0';                                                // convert to ASCII
}

// Convert ASCII array of length Len to BCD.
void Asc2Bcd(char *a, unsigned char Len)			
{
    unsigned char i;

    for (i = 0; i < Len; i++)
        a[i] &= 0x0F;                                               // convert to BCD
}

// Convert 3-digit ASCII array to binary
void Asc2Bin(char *a, unsigned int *reg)  			
{
    *reg = (int) (a[0] & 0x0F)*100 + (a[1] & 0x0F)*10 + (a[2] & 0x0F);
}

// Convert 4-digit ASCII array to binary
void Asc2BinE(char *a, unsigned int *reg)  		
{
    *reg = (int) (a[0] & 0x0F)*1000 + (a[1] & 0x0F)*100 + (a[2] & 0x0F)*10 + (a[3] & 0x0F);
}

// packed BCD to 2 ASCII bytes
void PBCD2Asc(unsigned char SrcReg, char *DstArr)							
{
    *DstArr++ = (SrcReg >> 4) | '0';                                // upper nibble to ASCII
    *DstArr-- = (SrcReg & 0x0F) | '0';                              // lower nibble to ASCII
}

#endif
#endif


