/*************************************************************************
*                               NASsie
*                      Raspberry Pi based OMV NAS
*
*   This program drives the LCD display on the NASsie NAS. It uses the 2
* buttons to control what is displayed. It assumes LVM2 is used and there
* are 2 LVM volumes. It tries to be resource light by being written in C
* and only using external programs when required. In general, it tries to
* use file only for information.
* LCD routines in the LCD directory are from Waveshare.
*
*
* TODO: (things to fix or update)
* -New brighter color scheme for screens
* -Figure out why program takes 100% of CPU core?
* -Add more screens (USB stick unmount, etc. using 2nd button)
* -Get new LCD routines (ideally AdaFruit on Pi SPI)
*--------------------------------------------------------------------------
* Copyright (c) 2024, Jeffrey Loeliger
* All rights reserved.
*
* This source code is licensed under the BSD-style license found in the
* LICENSE file in the root directory of this source tree.
*************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   //sleep
#include <string.h>
#include <signal.h>
#include <lgpio.h>
#include "NASsie_utils.h"
#include "./LCD/DEV_Config.h"
#include "./LCD/GUI_Paint.h"
#include "./LCD/GUI_BMP.h"
#include "./LCD/LCD_2inch4.h"
#include "./pic/NASsie_splash.h"  //splash screen image
#include "./pic/NASsie_stat.h"    //background for status screen
#include "./pic/NASsie_temp.h"    //background for temperature screen

#define BUFFER_SIZE 200

//#define NASSIE_DEBUG

#if defined(NASSIE_DEBUG)
#define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG: " fmt, ##args)
#else
#define DEBUG_PRINT(fmt, args...)  //do nothing for non-debug build
#endif

//Support functions in this file
void NASsie_handler(int signo);
void NASsie_button_left();
void NASsie_button_right();
void NASsie_update_LCD_stat();
void NASsie_update_LCD_temperature();
void NASsie_fan_update();
void sleep_count(int count);

enum state_type {splash, stats, temperature, standby};
enum state_type state;
int lgpio, status, fan;
int Temp_dev_max_sd[4], Temp_dev_min_sd[4];
unsigned int tick = 0, tick_slow = 0, standby_count = 0;
FILE *log_file;
time_t curtime;
char image[320*240*2];
UWORD *image_p;

//Variables from utility functions
extern int GPIO_Handle;
extern int Temp_CPU, Temp_dev_sd[4];
extern int Used_mem, Used_ssds, Used_hdds, Used_sdcard;
extern int CPU_load[4], lastSum[4], lastIdle[4]; //one for each CPU core
extern char wlan_ip[BUFFER_SIZE], eth_ip[BUFFER_SIZE];
extern char Size_mem[BUFFER_SIZE], Size_ssds[BUFFER_SIZE], Size_hdds[BUFFER_SIZE], Size_sdcard[BUFFER_SIZE];

int main()
{
	static int userdata20=123;
	static int userdata21=123;

	signal(SIGINT, NASsie_handler); // Exception handling:ctrl + c
	signal(SIGKILL, NASsie_handler); // Exception handling: kill signal
	signal(SIGTERM, NASsie_handler); // Exception handling: terminal signal

	state = splash;
	image_p = (UWORD *)image;

	/* LCD Module Init */
	if(DEV_ModuleInit() != 0) {
		DEV_ModuleExit();
		exit(0);
	}

	LCD_2IN4_Init();
	LCD_2IN4_Clear(WHITE);
	LCD_SetBacklight(1023);
	Paint_SetRotate(IMAGE_ROTATE_180 );
	LCD_2IN4_Display((UBYTE *)NASsie_splash);

	/* Configure backback button functions */
	status = lgGpioClaimInput(lgpio, LG_SET_PULL_DOWN, 20);
	lgGpioSetDebounce(lgpio, 20, 200000); // set 200 milliseconds of debounce
	lgGpioSetAlertsFunc(lgpio, 20, NASsie_button_right, &userdata20);
	status = lgGpioClaimAlert(lgpio, LG_SET_PULL_DOWN, LG_RISING_EDGE, 20, -1);

	status = lgGpioClaimInput(lgpio, LG_SET_PULL_DOWN, 21);
	lgGpioSetDebounce(lgpio, 21, 200000); // set 200 milliseconds of debounce
	lgGpioSetAlertsFunc(lgpio, 21, NASsie_button_left, &userdata21);
	status = lgGpioClaimAlert(lgpio, LG_SET_PULL_DOWN, LG_RISING_EDGE, 21, -1);

	/* fan */
	status = lgTxPwm(lgpio, 4, 100.0, 0.0, 0, 0); //100% default, fan signal is inverted

	/* get initial values */
	Update_Temp_CPU();
	Update_Used_mem();
	Update_Used_fs();
	Update_Network();
	Update_Load_CPU(); //Load will be wrong values but will work next call

	/* get initial drive temperatures */
	NASsie_fan_update();
	Temp_dev_max_sd[0] = Temp_dev_min_sd[0] = Temp_dev_sd[0];
	Temp_dev_max_sd[1] = Temp_dev_min_sd[1] = Temp_dev_sd[1];
	Temp_dev_max_sd[2] = Temp_dev_min_sd[2] = Temp_dev_sd[2];
	Temp_dev_max_sd[3] = Temp_dev_min_sd[3] = Temp_dev_sd[3];

	/* update screen, based on state:
		splash: every 30s update slow data
		stats: update fast data every 1s, slow data every 30s
		history: update data every 5s, slow data every 30s
		standby: update slow data every 30s
	*/
	while(1) {
		if (state != standby) {
			switch (state) {
				case stats:						//update every 1s
					NASsie_update_LCD_stat();
					Update_Load_CPU();
					Update_Used_mem();
					DEBUG_PRINT("stat update\n");
					break;
				case temperature:					//update every 10s (or 5s?)
					if(tick > 5) {
						NASsie_update_LCD_temperature();
						NASsie_fan_update();
						tick = 0;
						DEBUG_PRINT("tickupdate\n");
					}
					break;
				default:
					LCD_2IN4_Display((UBYTE *)NASsie_splash);
			}
			LCD_SetBacklight(1023); //turn backlight on in case in standby
			if (standby_count > 300) {		//every 5 minutes (300 seconds)
				state = standby;
				LCD_SetBacklight(0);
				standby_count = 0;
			}
			tick++;
			standby_count++;
		}
		tick_slow++;
		if(tick_slow > 30) {			//update every 30 seconds
			NASsie_fan_update();
			Update_Temp_CPU();
			Update_Used_fs();
			Update_Network();
			tick_slow=0;
			DEBUG_PRINT("tick_slow update\n");
		}
		sleep(1);
	}
}

/***************************************************************************
*SUMMARY: Callback function for left button. When button is pressed change state.
*
*  Parameters: event number, alart array and data pointer
*  Return: none
*  Globals: state, standby_count
****************************************************************************/
void NASsie_button_left(int e, lgGpioAlert_p evt, void *data)
{
	switch (state) {
		case splash:
			state = stats;
			break;
		case stats:
			state = temperature;
			break;
		default:
			state = splash;
	}
	standby_count = 0; //come out of standby mode when button is pressed
}

/***************************************************************************
*SUMMARY: Callback routine for right button, ***currently not used***
*
*  Parameters: event number, alart array and data pointer
*  Return: none
*  Globals: none
****************************************************************************/
void NASsie_button_right(int e, lgGpioAlert_p evt, void *data)
{
}

/***************************************************************************
*SUMMARY: Update LCD with stats screen
*
*  Parameters: none
*  Return: none
*  Globals: ...
****************************************************************************/
void NASsie_update_LCD_stat()
{
	int x, color;

	memcpy((void *)image_p, (const void *) NASsie_stat, sizeof(NASsie_stat));
	Paint_NewImage(image_p, LCD_2IN4_WIDTH, LCD_2IN4_HEIGHT, 0, WHITE, 16);
	Paint_SetRotate(ROTATE_180);

//	CPU Load
	x = CPU_load[0];
	x = (x*150)/100;
	if (x<0) x = 0;
	if (x>150) x = 150;
	Paint_DrawRectangle(65, 70, 65+x, 82, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
	x = CPU_load[1];
	x = (x*150)/100;
	Paint_DrawRectangle(65, 84, 65+x, 96, GRAY, DOT_PIXEL_1X1, DRAW_FILL_FULL);
	x = CPU_load[2];
	x = (x*150)/100;
	Paint_DrawRectangle(65, 98, 65+x, 110, BRED, DOT_PIXEL_1X1, DRAW_FILL_FULL);
	x = CPU_load[3];
	x = (x*150)/100;
	Paint_DrawRectangle(65, 112, 65+x, 124, BROWN, DOT_PIXEL_1X1, DRAW_FILL_FULL);

// CPU temperature
	if (Temp_CPU<55) color = GREEN;
	else if (Temp_CPU<70) color = YELLOW;
	else color = RED;
	x = ((Temp_CPU - 20)*150)/60;
	if (x<0) x = 0;
	if (x>150) x = 150;
	Paint_DrawRectangle(65, 142, 65+x, 154, color, DOT_PIXEL_1X1, DRAW_FILL_FULL);

//STORAGE
	x = ((Used_sdcard*150)/100);
	Paint_DrawRectangle(65, 203, 65+x, 215, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
	if (Used_hdds != -1) {
		x = ((Used_hdds*150)/100)+1;   //make sure there is at least 1 bar
		Paint_DrawRectangle(65, 217, 65+x, 229, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
	}
	if (Used_ssds != -1) {
		x = ((Used_ssds*150)/100)+1;
		Paint_DrawRectangle(65, 231, 65+x, 242, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
	}

//IP addresses
	Paint_DrawString_EN(59, 280, (const char *) eth_ip, &Font16, WHITE, BLACK);
	Paint_DrawString_EN(59, 296, (const char *) wlan_ip, &Font16, WHITE, BLACK);
	LCD_2IN4_Display((UBYTE *)image);
}

/***************************************************************************
*SUMMARY: Update LCD with temperature screen
*
*  Parameters: none
*  Return: none
*  Globals: Temp_dev_sd[],Temp_dev_min_sd[],Temp_dev_max_sd[],fan
****************************************************************************/
void NASsie_update_LCD_temperature()
{
	memcpy((void *)image_p, (const void *) NASsie_temp, sizeof(NASsie_temp));
	Paint_NewImage(image_p, LCD_2IN4_WIDTH, LCD_2IN4_HEIGHT, 0, WHITE, 16);
	Paint_SetRotate(ROTATE_180);

	//sda
	Paint_DrawNum(90, 115, Temp_dev_min_sd[0], &Font20, WHITE, BLACK);
	Paint_DrawNum(140, 115, Temp_dev_sd[0], &Font20, WHITE, BLACK);
	Paint_DrawNum(190, 115, Temp_dev_max_sd[0], &Font20, WHITE, BLACK);

	//sdb
	Paint_DrawNum(90, 141, Temp_dev_min_sd[1], &Font20, WHITE, BLACK);
	Paint_DrawNum(140, 141, Temp_dev_sd[1], &Font20, WHITE, BLACK);
	Paint_DrawNum(190, 141, Temp_dev_max_sd[1], &Font20, WHITE, BLACK);

	//sdc
	Paint_DrawNum(90, 169, Temp_dev_min_sd[2], &Font20, WHITE, BLACK);
	Paint_DrawNum(140, 169, Temp_dev_sd[2], &Font20, WHITE, BLACK);
	Paint_DrawNum(190, 169, Temp_dev_max_sd[2], &Font20, WHITE, BLACK);

	//sdd
	Paint_DrawNum(90, 196, Temp_dev_min_sd[3], &Font20, WHITE, BLACK);
	Paint_DrawNum(140, 196, Temp_dev_sd[3], &Font20, WHITE, BLACK);
	Paint_DrawNum(190, 196, Temp_dev_max_sd[3], &Font20, WHITE, BLACK);

	//fan
	if(fan==0)
		Paint_DrawString_EN(110, 258, "OFF", &Font24, WHITE, BLACK);
	else
		Paint_DrawNum(125, 258, fan, &Font24, WHITE, BLACK);

	LCD_2IN4_Display((UBYTE *)image);
}

/***************************************************************************
*SUMMARY: This function updates the fan speed based on the hottest drive
*
*  Parameters: none
*  Return: none
*  Globals: fan (fan speed in percent)
****************************************************************************/
void NASsie_fan_update()
{
	int i;
	Update_Temp_SMART();

	fan = 0;

	for (i=0; i<4; i++) {
		if (Temp_dev_sd[i] < Temp_dev_min_sd[i]) Temp_dev_min_sd[i] = Temp_dev_sd[i];
		if (Temp_dev_sd[i] > Temp_dev_max_sd[i]) Temp_dev_max_sd[i] = Temp_dev_sd[i];
		switch(Temp_dev_sd[i]) {
			case 0 ... 35:				//Ideal temperature range
				if(fan < 1) fan = 0;
				break;
			case 36 ... 38:
				if(fan < 51) fan = 50;
				break;
			case 39 :
				if(fan < 61) fan = 60;
				break;
			case 40 :					//too hot but not damgerous
				if(fan < 71) fan = 70;
				break;
			case 41 :
				if(fan < 81) fan = 80;
				break;
			case 42 :
				if(fan < 91) fan = 90;
				break;
			default:
				fan = 100;
		}
	}
//	fan=100;  //DEBUG set to 100% while debugging
	i = lgTxPwm(lgpio, 4, 100.0, (100.0-fan), 0, 0);
	DEBUG_PRINT("fan %i\n", fan);
}

/***************************************************************************
*SUMMARY:
*  Signal handler. Shutdown requested so end program cleanly.
*  The signal number (signo) is ignored since the program always ends
*    -Clear LCD
*    -Turn off LCD backlight
*    -Shutdown LCD/lpgio system
*    -exit program
*
*  Parameters: signo (signal number)
*  Return: none
*  Globals: none
****************************************************************************/
void  NASsie_handler(int signo)
{
	LCD_2IN4_Clear(BLACK);
	LCD_SetBacklight(0);
	DEV_ModuleExit();
	exit(0);
}