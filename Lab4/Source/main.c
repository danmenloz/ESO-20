/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#include <MKL25Z4.H>
#include <stdio.h>
#include "gpio_defs.h"
#include <cmsis_os2.h>
#include "threads.h"

#include "LCD.h"
#include "LCD_driver.h"
#include "font.h"

#include "LEDs.h"
#include "timers.h"
#include "sound.h"
#include "DMA.h"
#include "I2C.h"
#include "mma8451.h"
#include "delay.h"
#include "profile.h"
#include "math.h"

/*----------------------------------------------------------------------------
  MAIN function
 *----------------------------------------------------------------------------*/
int main (void) {

	Init_Debug_Signals();
	Init_RGB_LEDs();
	Control_RGB_LEDs(0,0,1);			

	Sound_Init();	
	Play_Tone();
	Sound_Disable_Amp();
	
	LCD_Init();
	LCD_Text_Init(1);
	LCD_Erase();
	
	LCD_Text_PrintStr_RC(0,0, "Test Code");

	LCD_Text_PrintStr_RC(1,0, "Accel...");
	i2c_init();											// init I2C peripheral
	if (!init_mma()) {							// init accelerometer
		I2C_OK = 0;	// code won't read accelerometer
		// Initialize variables for fake data
		acc_X = 0;
		acc_Y = 0;
		acc_Z = -4000;
	} else {
		I2C_OK = 1;
	}
	LCD_Text_PrintStr_RC(1,9, "Done");

	Delay(70);
	LCD_Erase();

	osKernelInitialize();
	Create_OS_Objects();
	osKernelStart();	
	while(1);
}
