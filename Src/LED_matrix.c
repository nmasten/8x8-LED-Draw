/*
 * LED_matrix.c
 *
 *  Created on: Jun 7, 2023
 *      Author: noahmasten
 */

#include "LED_Matrix.h"
#include "math.h"
#include <stdint.h>

/*
 * Stores LED number and RGB values in array
 *
 * Source:
 * https://controllerstech.com/interface-ws2812-with-stm32/
 */
void Set_LED (int LEDnum, int Red, int Green, int Blue)
{
	LED_Data[LEDnum][0] = LEDnum;
	LED_Data[LEDnum][1] = Green;
	LED_Data[LEDnum][2] = Red;
	LED_Data[LEDnum][3] = Blue;
}

/*
 * Sets brightness of LED
 *
 * Source:
 * https://controllerstech.com/interface-ws2812-with-stm32/
 */
void Set_Brightness (int brightness)  // 0-45
{
#if USE_BRIGHTNESS

	if (brightness > 45) brightness = 45;
	for (int i=0; i<MAX_LED; i++)
	{
		LED_Mod[i][0] = LED_Data[i][0];
		for (int j=1; j<4; j++)
		{
			float angle = 90-brightness;  // in degrees
			angle = angle*PI / 180;  // in rad
			LED_Mod[i][j] = (LED_Data[i][j])/(tan(angle));
		}
	}

#endif
}

/*
 * Resets all LEDs to RGB value (0, 0, 0)
 *
 * Source:
 * https://controllerstech.com/interface-ws2812-with-stm32/
 */
void Reset_LED (void)
{
	for (int i=0; i<MAX_LED; i++)
	{
		LED_Data[i][0] = i;
		LED_Data[i][1] = 0;
		LED_Data[i][2] = 0;
		LED_Data[i][3] = 0;
	}
}


