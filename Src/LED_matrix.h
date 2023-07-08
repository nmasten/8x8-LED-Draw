/*
 * LED_matrix.h
 *
 *  Created on: Jun 7, 2023
 *      Author: noahmasten
 */

#ifndef SRC_LED_MATRIX_H_
#define SRC_LED_MATRIX_H_

#include <stdint.h>

#define MAX_LED 64			// number of LEDs
#define USE_BRIGHTNESS 1	// 1 or 0, depending on if we want to change brightness or not
#define MATRIX_WIDTH 8		// width of LED panel
#define MATRIX_HEIGHT 8		// height of LED panel
#define PI 3.14159265

extern uint8_t LED_Matrix[MATRIX_WIDTH][MATRIX_HEIGHT];
extern uint8_t LED_Data[MAX_LED][4];
extern uint8_t LED_Mod[MAX_LED][4];

void Set_LED (int LEDnum, int Red, int Green, int Blue);
void Set_Brightness (int brightness);
void Reset_LED (void);

#endif /* SRC_LED_MATRIX_H_ */
