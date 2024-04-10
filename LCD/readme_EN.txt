/*****************************************************************************
* | File      	:   Readme_EN.txt
* | Author      :   Waveshare team
* | Function    :   Help with use
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-05-21
* | Info        :   Here is an English version of the documentation for your quick use.
******************************************************************************/
This file is to help you use this routine.

1. Basic information:
This routine has been verified using the Separate LCD module. 
You can view the corresponding test routines in the \Examples\ of the project.
This Demo has been verified on the Raspberry Pi 4B;

2. Pin connection:
Pin connection You can view it in DEV_Config.h in the \lib\Config\ directory, and repeat it here:
EPD  	=>	RPI(BCM)
VCC    	->    	5V
GND    	->    	GND
DIN    	->    	10(SPI0_MOSI)
CLK    	->    	11(SPI0_SCK)
CS     	->    	8(CE0)
DC     	->    	25
RST    	->    	27
BL  	->    	18

3. Basic use:
you need to execute: 
    make
compile the program, will generate the executable file: main
 If you purchased 1.54inch LCD Module, then you should execute the command:
    sudo ./main 1.54
If you modify the program, you need to execute: 
	make clear
then:
    make

4. Directory structure (selection):
If you use our products frequently, we will be very familiar with our program directory structure. We have a copy of the specific function.
The API manual for the function, you can download it on our WIKI or request it as an after-sales customer service. Here is a brief introduction:
Config\: This directory is a hardware interface layer file. You can see many definitions in DEV_Config.c(.h), including:
   type of data;
    GPIO;
    Read and write GPIO;
    Delay: Note: This delay function does not use an oscilloscope to measure specific values.
    Module Init and exit processing:
        void DEV_Module_Init(void);
        void DEV_Module_Exit(void);
            
\lib\GUI\: This directory is some basic image processing functions, in GUI_Paint.c(.h):
    Common image processing: creating graphics, flipping graphics, mirroring graphics, setting pixels, clearing screens, etc.
    Common drawing processing: drawing points, lines, boxes, circles, Chinese characters, English characters, numbers, etc.;
    Common time display: Provide a common display time function;
    Commonly used display pictures: provide a function to display bitmaps;
    
\lib\Fonts\: for some commonly used fonts:
    Ascii:
        Font8: 5*8
        Font12: 7*12
        Font16: 11*16
        Font20: 14*20
        Font24: 17*24
    Chinese:
        font12CN: 16*21
        font24CN: 32*41
        
\lib\LCD\: This screen is the LCD driver function;
Examples\: This is the test program for the LCD. You can see the specific usage method in it.