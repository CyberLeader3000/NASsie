# NASsie
## Raspberry Pi CM4 based home NAS running OMV.

This program drives the LCD display on the NASsie NAS. It uses the 2 buttons to control what is displayed. It assumes LVM2 is used and there are 2 LVM volumes. It tries to be resource light by being written in C and only using external programs when required. In general, it tries to use files only for information.

LCD routines in the LCD directory are from Waveshare.
(https://www.waveshare.com/wiki/2.4inch_LCD_Module)


 TODO: (things to fix or update)
 - New brighter color scheme for screens
 - Figure out why program takes 100% of CPU core?
 - Add more screens (USB stick unmount, etc. using 2nd button)
 - Get new LCD routines (ideally AdaFruit on Pi SPI)
