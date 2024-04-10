/*************************************************************************
*                             NASsie_utils
*                      NASsie utility functions
*
*   These are utility functions to support the NASsie LCD display.
*
*--------------------------------------------------------------------------
* Copyright (c) 2024, Jeffrey Loeliger
* All rights reserved.
*
* This source code is licensed under the BSD-style license found in the
* LICENSE file in the root directory of this source tree.
*************************************************************************/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 200

/* GLOBAL VARIBLES */
int Temp_CPU, Temp_dev_sd[4];
int Used_mem, Used_ssds, Used_hdds, Used_sdcard;
int CPU_load[4], lastSum[4], lastIdle[4]; //one for each CPU core
char wlan_ip[BUFFER_SIZE], eth_ip[BUFFER_SIZE];
char Size_mem[BUFFER_SIZE], Size_ssds[BUFFER_SIZE], Size_hdds[BUFFER_SIZE], Size_sdcard[BUFFER_SIZE]; //small strings

/***************************************************************************
*SUMMARY:
*  Update global CPU temperature variable with CPU temperature in Celcius
*  Read temperature in milli-degress from system file.
*
*  Parameters: none
*  Return: error code, not currently used.
*  Globals: Temp_CPU
****************************************************************************/
int Update_Temp_CPU()
{
	char buffer[BUFFER_SIZE];

	FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");

	if (fp == NULL) return(1);
	fgets(buffer, sizeof buffer, fp);
	fclose(fp);       //close the file

	Temp_CPU = (atoi(buffer)/1000);	//convert ascii milli-degrees to number */
	return(0);
}

/***************************************************************************
*SUMMARY:
*  Update global drive temperature variables with drive temperature in Celcius.
*  Function may be slow because it needs data from the drive itself.
*
*  Parameters: none
*  Return: error code, not currently used.
*  Globals: Temp_dev_sd[0],Temp_dev_sd[1],Temp_dev_sd[2],Temp_dev_sd[3]
****************************************************************************/
int Update_Temp_SMART()
{
	char buffer[BUFFER_SIZE];
	const char* command[4] = { "hddtemp -w /dev/sda", "hddtemp -w /dev/sdb",
	                           "hddtemp -w /dev/sdc", "hddtemp -w /dev/sdd"
	                         };
	const char* device[4] = {"/dev/sda","/dev/sdb","/dev/sdc","/dev/sdd"};
	char *temperature;
	int i;

	for (i=0; i<4; i++) {
		if ( access( device[i],F_OK)==0) {   //does the drive exit
			FILE *fp = popen(command[i], "r");
			if (fp != NULL) {
				fgets( buffer, BUFFER_SIZE, fp);
				temperature = strtok(buffer, ":");
				temperature = strtok(NULL, ":");
				temperature = strtok(NULL, ":");
				Temp_dev_sd[i] = atoi(temperature);
			}
			pclose(fp);
//		printf("%s %i\n",command[i],Temp_dev_sd[i]);
		} else
			Temp_dev_sd[i] = 0;
	};
	return(0);
}

/***************************************************************************
*SUMMARY: Update global memory used by CPU and return as percentage.
*
*  Parameters: none
*  Return: error code, not currently used.
*  Globals: Used_mem
****************************************************************************/
int Update_Used_mem()
{
	char buffer[BUFFER_SIZE];
	char *size;
	double t, a;

	FILE *fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) return(1);

	fgets(buffer, sizeof buffer, fp); //total memory

	/* Determine percentage used */
	fgets(buffer, sizeof buffer, fp);  //ignore free value
	fgets(buffer, sizeof buffer, fp);  //available memory
	fclose(fp);
	size = strtok(buffer, ":");
	size = strtok(NULL, ":");
	a = atof(size);
	t = atof(Size_mem);
	Used_mem = (lround((1.0-(a/t))* 100.0));
	return(0);
}

/***************************************************************************
*SUMMARY:
*  Update global variable network addresses for both interfaces.
*
*  Parameters: none
*  Return: error code, not currently used.
*  Globals: wlan_ip, eth_ip
****************************************************************************/
int Update_Network()
{
	char buffer[BUFFER_SIZE];
	char *tok;
	FILE *fp;

	/* wlan0 */
	fp = popen("ifconfig wlan0", "r");
	fgets( buffer, sizeof buffer, fp);
	fgets( buffer, sizeof buffer, fp);
	pclose(fp);

	tok = strtok(buffer, " ");
	if (strcmp(tok, "inet") == 0) {
		tok = strtok(NULL, " ");
		strcpy(wlan_ip, tok);
	} else {
		wlan_ip[0] = '\0';
	}

	/* eth0 */
	fp = popen("ifconfig eth0", "r");
	fgets( buffer, sizeof buffer, fp);
	fgets( buffer, sizeof buffer, fp);
	pclose(fp);

	tok = strtok(buffer, " ");
	if (strcmp(tok, "inet") == 0) {
		tok = strtok(NULL, " ");
		strcpy(eth_ip, tok);
	} else {
		eth_ip[0] = '\0';
	}

	return(0);
}

/***************************************************************************
*SUMMARY:
*  Update amount of file system used by device in percentage.
*
*  Parameters: none
*  Return: error code, not currently used.
*  Globals: wlan_ip, eth_ip
****************************************************************************/
int Update_Used_fs()
{
	static char buffer[BUFFER_SIZE];
	FILE *fp;

	if (access("/dev/dm-0",F_OK) == 0) {
		fp = popen("df --output=pcent /dev/dm-0", "r");
		if (fp == NULL) return(1);
		fgets( buffer, sizeof buffer, fp);
		fgets( buffer, sizeof buffer, fp);
		fclose(fp);
		Used_hdds = (atoi(buffer));
	} else Used_hdds = -1;

	fp = popen("df --output=pcent /", "r");
	if (fp == NULL) return(1);
	fgets( buffer, sizeof buffer, fp);
	fgets( buffer, sizeof buffer, fp);
	fclose(fp);
	Used_sdcard = (atoi(buffer));

	if (access("/dev/dm-1",F_OK) == 0) {
		fp = popen("df --output=pcent /dev/dm-1", "r");
		if (fp == NULL) return(1);
		fgets( buffer, sizeof buffer, fp);
		fgets( buffer, sizeof buffer, fp);
		fclose(fp);
		Used_ssds = (atoi(buffer));
	} else Used_ssds = -1;

	return(0);
}

/***************************************************************************
*SUMMARY:
* Determine the percentage busy for all CPUs:
*  1. read /proc/stat
*  2. select line for CPU (total, CPU0, CPU1, CPU2, CPU3)
*  3. discard the first word of that first line   (it's always cpu)
*  4. sum all of the times found on that first line to get the total time
*  5. divide the fourth column ("idle") by the total time, to get the fraction of time spent being idle
*  6. subtract the previous fraction from 1.0 to get the time spent being   not   idle
*  7. multiple by   100   to get a percentage
*  8. subtract from 100 to get busy percentage
*
*  Parameters: none
*  Return: error code, not currently used.
*  Globals: ...
****************************************************************************/

int Update_Load_CPU()
{
	char buffer[BUFFER_SIZE];
	char* token;
	const char d[2] = " ";     //string delimiter characters
	int i,j;
	FILE* fp;
	long int sum = 0, idle, a, b;
	double idleFraction, x, y, z;

//Load buffer with the line for the requested CPU
	fp = fopen("/proc/stat","r");
	fgets(buffer, sizeof buffer, fp); //read & ignore all CPUs combined
	for (i = 0; i < (4); i++) {
		fgets(buffer, sizeof buffer, fp);
//		    printf("CPU %i : %s\n",i,buffer);  //DEBUG
		token = strtok(buffer,d);    //ignore CPUx characters
		j=0;
		sum = 0;
		while(token!=NULL) {
			token = strtok(NULL,d);
			if(token!=NULL) {
				sum += atoi(token);
				if(j==3)
					idle = atoi(token);
				j++;
			}
		}
		//	printf("idle: %ld  lastidle: %ld  sum: %ld lastsum: %ld\n", idle, lastIdle, sum, lastSum);

		a = (idle)-lastIdle[i];
		b = (sum)-lastSum[i];
		x = a;
		y = b;
		z = (x / y) * 100.0;

//		printf("CPU %i a: %i   b: %i  z: %f\n",i, a, b, z);
		idleFraction = 100.0 - z;
//		printf("CPU %i Busy: %f %%, a: %i b: %i.\n",i, idleFraction,a,b);

		lastIdle[i] = idle;
		lastSum[i] = sum;
		CPU_load[i]= idleFraction;
	}
	fclose(fp);

	return(0);
}

