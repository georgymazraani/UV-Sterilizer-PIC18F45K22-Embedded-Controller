#include <p18cxxx.h>
#include <delays.h>
#include <BCDlib.h>
#include <LCDnew.h>

char SSCODES [] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
char counter = 0; //max 20 s which is 200*100ms, char is a good fit
char seconds = 0, countDown = 0, finish = 0;
char state = 0;
char time_add = 0;
char level = 0;
unsigned char digits[] = {0, 0, 0};

void Idle(void);
void Display(void);
void WaitDelay(void);
void SterProcess(void);
void Ventilation(void);
void Update_Display(char stage);
void time_add_method(char level);

void Setup(void) {
    TRISD = 0x00; //SEVEN segment output
    ANSELD = 0x00; // digital

    TRISC &= 0xF0; //UV, Fan, 2 enable lines configured as outputs 
    ANSELC &= 0xF0; //digital

    TRISBbits.RB0 = 1;
    ANSELBbits.ANSB0 = 0;
    WPUBbits.WPUB0 = 1;

    TRISBbits.RB1 = 1;
    ANSELBbits.ANSB1 = 0;
    WPUBbits.WPUB1 = 1;

    TRISBbits.RB2 = 1;
    ANSELBbits.ANSB2 = 0;
    WPUBbits.WPUB2 = 1;

    TRISBbits.RB3 = 1;
    ANSELBbits.ANSB3 = 0;
    WPUBbits.WPUB3 = 1;


    INTCON2bits.RBPU = 0;

    INTCONbits.GIE = 1;
    INTCONbits.T0IE = 1;
    T0CON = 0b11010100; // prescaler 1:32 and 8 bit mode
    TMR0L = (256 - 250); //8 ms refresh time


    InitLCD();
    Update_Display(1);
}

void main(void) {
    Setup();
    while (1) {
        while (1) {
            Update_Display(1);
            Idle();
            WaitDelay();
            SterProcess();
            if (!finish) break;
            Ventilation();
        }
    }
}

#pragma code ISR=0x0008
#pragma interrupt ISR

void ISR(void) {
    INTCONbits.T0IF = 0;
    TMR0L = (256 - 250);
    if (countDown) {
        if (counter == 124) {
            counter = 0;
            seconds--;
            Bin2Bcd(seconds, digits);
        } else
            counter++;
    }
    Display();
}

void Idle(void) {//we should clear the digits before
    PORTC &= 0xFC; //turn off both uv and Fan
    digits[1] = 0, digits[2] = 0;
    if (!PORTBbits.RB2)
        level = 0;
    else if (!PORTBbits.RB3)
        level = 1;
    time_add_method(level);
    while (PORTBbits.RB0); //while push button not pressed
    while (!PORTBbits.RB0); //wait until pushbutton is released
    counter = 0; //clean 20s


}

void Display(void) {//change digits before
    state = !(state);
    //clear both enable lines
    PORTCbits.RC2 = 0;
    PORTCbits.RC3 = 0;
    //prepare the digits
    PORTD = SSCODES[digits[1 + state]]; //if RC2=1, state=1, I need the last digit SSCODES[digits[2]]  
    //turn the appropriate LED
    PORTCbits.RC2 = state; // we have "NOT" in the hardware to make it active high D
    PORTCbits.RC3 = !state;
}

void WaitDelay(void) {
    INTCONbits.T0IE = 0;
    TMR0L = (256 - 250);
    INTCONbits.T0IE = 1;
    Update_Display(2);
    seconds = 5;
    Bin2Bcd(seconds, digits);
    countDown = 1;
    while (seconds != 0);
    countDown = 0;

}

void SterProcess(void) {
    Update_Display(3);
    PORTCbits.RC0 = 1; //UV on

    INTCONbits.T0IE = 0;
    TMR0L = (256 - 250);
    INTCONbits.T0IE = 1;

    seconds = 20 + time_add;
    Bin2Bcd(seconds, digits); //initial number
    countDown = 1;
    finish = 1;
    while (seconds != 0) {
        if (PORTBbits.RB1) { //nc sensor
            Bin2Bcd(0, digits);
            PORTCbits.RC0 = 0;
            finish = 0;
            countDown = 0;
            Update_Display(5);
            Delay10KTCYx(100);
            break;
        }
    }
    counter = 0;
    countDown = 0;
}

void Ventilation(void) {
    Update_Display(4);
    PORTCbits.RC0 = 0;
    INTCONbits.T0IE = 0;
    TMR0L = (256 - 250);
    INTCONbits.T0IE = 1;

    PORTCbits.RC1 = 1; //Fan is turned ON
    seconds = 10 + time_add;
    Bin2Bcd(seconds, digits); //initial number
    countDown = 1;
    while (seconds != 0);
    PORTCbits.RC1 = 0; //Fan is turned OFF
    countDown = 0;
}

void Update_Display(char stage) {
    DispBlanks(Ln1Ch0, 16);
    DispBlanks(Ln2Ch0, 16);

    switch (stage) {
        case 1:
            DispRomStr(Ln1Ch0, (ROM) "Welcome");
            DispRomStr(Ln2Ch0, (ROM) "Press button");
            break;

        case 2:
            DispRomStr(Ln1Ch0, (ROM) "Stage 1:");
            DispRomStr(Ln2Ch0, (ROM) "Preparing");
            break;

        case 3:
            DispRomStr(Ln1Ch0, (ROM) "Stage 2:");
            DispRomStr(Ln2Ch0, (ROM) "UV Sterilization");
            break;

        case 4:
            DispRomStr(Ln1Ch0, (ROM) "Stage 3:");
            DispRomStr(Ln2Ch0, (ROM) "Ventilation");
            break;

        case 5:
            DispRomStr(Ln1Ch0, (ROM) "Abort!");
            DispRomStr(Ln2Ch0, (ROM) "Door is opened");
            break;
    }
}

void time_add_method(char level) {
    switch (level) {
        case 0:
            time_add = 0;
        case 1:
            time_add = 10;
    }