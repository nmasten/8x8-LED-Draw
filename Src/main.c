#include "main.h"
#include "math.h"
#include "uart.h"
#include "LED_matrix.h"

#define DEFAULT_BRIGHTNESS 1	// The brightness value used for this program, anything higher is too bright

uint8_t LED_Data[MAX_LED][4];	// Stores LED number and RGB
uint8_t LED_Mod[MAX_LED][4];	// Stores LED number and RGB, used only when USE_BRIGHTNESS is enabled (1)

// Initialize user interface grid
char UI_Matrix[MATRIX_WIDTH][MATRIX_HEIGHT] = {
		{ '*', '*', '*', '*', '*', '*', '*', '*' },
		{ '*', '*', '*', '*', '*', '*', '*', '*' },
		{ '*', '*', '*', '*', '*', '*', '*', '*' },
		{ '*', '*', '*', '*', '*', '*', '*', '*' },
		{ '*', '*', '*', '*', '*', '*', '*', '*' },
		{ '*', '*', '*', '*', '*', '*', '*', '*' },
		{ '*', '*', '*', '*', '*', '*', '*', '*' },
		{ '*', '*', '*', '*', '*', '*', '*', '*' }
};

// Initialize LED numbers to easily reference with x and y coordinates
uint8_t LED_Matrix[MATRIX_WIDTH][MATRIX_HEIGHT] = {
		{ 0, 15, 16, 31, 32, 47, 48, 63 },
		{ 1, 14, 17, 30, 33, 46, 49, 62 },
		{ 2, 13, 18, 29, 34, 45, 50, 61 },
		{ 3, 12, 19, 28, 35, 44, 51, 60 },
		{ 4, 11, 20, 27, 36, 43, 52, 59 },
		{ 5, 10, 21, 26, 37, 42, 53, 58 },
		{ 6, 9,  22, 25, 38, 41, 54, 57 },
		{ 7, 8,  23, 24, 39, 40, 55, 56 }
};

int datasentflag = 0;		// indicates when data has been sent to LEDs

TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_ch2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);

void print_matrix(uint8_t x_pos, uint8_t y_pos);

/*
 * Function called when DMA pulse is completed
 *
 * Source:
 * https://controllerstech.com/interface-ws2812-with-stm32/
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
	HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_2);					// Stop DMA transfer
	datasentflag = 1;												// Set data flag high
}

// Pulse width modulation data, stores 60 or 30, based on if data is a 1 or 0
uint16_t pwmData[(24 * MAX_LED) + 50];

/*
 * Function to send data to LEDs
 *
 * Source:
 * https://controllerstech.com/interface-ws2812-with-stm32/
 */
void WS2812_Send(void) {
	uint32_t indx = 0;
	uint32_t color;

	// Concatenate the color bits
	for (int i = 0; i < MAX_LED; i++) {
#if USE_BRIGHTNESS
		color = ((LED_Mod[i][1] << 16) | (LED_Mod[i][2] << 8) | (LED_Mod[i][3]));
#else
		color = ((LED_Data[i][1]<<16) | (LED_Data[i][2]<<8) | (LED_Data[i][3]));
#endif
		// write 60 if color bit is a 1, otherwise write 30
		for (int i = 23; i >= 0; i--) {
			if (color & (1 << i)) {
				pwmData[indx] = 60;  // 2/3 (~68%) of 90
			}

			else
				pwmData[indx] = 30;  // 1/3 (~32%) of 90

			indx++;
		}

	}

	// 50 us "reset code" after data has been sent
	for (int i = 0; i < 50; i++) {
		pwmData[indx] = 0;
		indx++;
	}

	HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t*) pwmData, indx);	// Send data
	while (!datasentflag);														// wait for DMA to finish
	datasentflag = 0;															// reset flag
}

int8_t x = 0, y = 0;		// current x and y coordinates on LED board
int key_pressed_flag = 0;	// indicates whether a key has been pressed
char keypress;				// stores value of keypress

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	HAL_Init();

	SystemClock_Config();

	// Init TIM1 Channel 2 for PWM and DMA (PA9)
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_TIM1_Init();

	// Init UART
	UART_Init();

	// Start with all LEDs turned off
	Reset_LED();
	Set_Brightness(DEFAULT_BRIGHTNESS);
	WS2812_Send();

	// initialize FSM states
	typedef enum {
		ST_DEFAULT, ST_MOVE_CURSOR, ST_SEL_COLOR, ST_DRAW, ST_ERASE, ST_CLEAR
	} state_var_type;

	// struct variable to store color info
	typedef struct {
		char name[10];	// color name
		uint8_t red;	// color R value, 0-255
		uint8_t green;	// color G value, 0-255
		uint8_t blue;	// color B value, 0-255
		char key;		// keypress associated with color switch
	} color;

	// Initialize usable colors
	color RED = { "RED", 255, 0, 0, 'R' };
	color ORANGE = { "ORANGE", 255, 127, 0, 'O' };
	color YELLOW = { "YELLOW", 255, 255, 0, 'Y' };
	color GREEN = { "GREEN", 0, 255, 0, 'G' };
	color BLUE = { "BLUE", 0, 0, 255, 'B' };
	color INDIGO = { "INDIGO", 75, 0, 130, 'I' };
	color VIOLET = { "VIOLET", 148, 0, 211, 'V' };
	color PINK = { "PINK", 255, 105, 180, 'P' };
	color BROWN = { "BROWN", 150, 75, 0, 'N' };
	color WHITE = { "WHITE", 255, 255, 255, 'W' };

	// Initial state and color
	state_var_type curr_state = ST_DEFAULT;
	color curr_color = WHITE;

	while (1) {
		switch (curr_state) {

		// Default state which prints User Interface
		case ST_DEFAULT:
			UART_ESC_Code("[2J");							// clear terminal
			UART_ESC_Code("[H");							// move cursor to top left
			UART_ESC_Code("[1m");							// bold text
			UART_ESC_Code("[34m");							// blue text
			UART_print("Welcome to 8x8 Draw!\r\n\r\n");		// print title
			UART_ESC_Code("[0m");							// turn off character attributes

			// Print controls section
			UART_print("Controls:\r\n");
			UART_print("W         Cursor Up\r\n");
			UART_print("A         Cursor Left\r\n");
			UART_print("S         Cursor Down\r\n");
			UART_print("D         Cursor Right\r\n");
			UART_print("C         Select Color\r\n");
			UART_print("Space     Draw Pixel\r\n");
			UART_print("E         Erase Pixel\r\n");
			UART_print("R         Reset LEDs\r\n");
			UART_print("\r\n");
			UART_print("Color: ");
			UART_print(curr_color.name);
			UART_print("\r\n");
			UART_print("x: ");
			UART_print_int(x + 1);
			UART_print(", y: ");
			UART_print_int(y + 1);

			// print UI grid, using current x and y values for the cursor placement
			UART_print("\r\n\r\n");
			print_matrix(x, y);

			while (!key_pressed_flag);		// wait for key to be pressed
			key_pressed_flag = 0;			// clear key press flag

			// switch FSM state based on keypress
			if (keypress == 'w' || keypress == 'a' || keypress == 's' || keypress == 'd') {		// if W, A, S, D, move cursor
				curr_state = ST_MOVE_CURSOR;
			} else if (keypress == 'c') {														// if C, select color
				curr_state = ST_SEL_COLOR;
			} else if (keypress == ' ') {														// if Space, draw pixel
				curr_state = ST_DRAW;
			} else if (keypress == 'e') {														// if E, erase pixel
				curr_state = ST_ERASE;
			} else if (keypress == 'r') {														// if R, reset LEDs
				curr_state = ST_CLEAR;
			} else {																			// otherwise, don't switch state
				curr_state = ST_DEFAULT;
			}
			break;

		// State to move cursor based on keypress
		case ST_MOVE_CURSOR:

			// Increment of decrement x/y coordinate, depending on the key pressed
			// Make sure 0 <= x,y <= 7
			switch (keypress) {
			case 'w':
				y--;
				y = (y < 0) ? 0 : y;
				break;
			case 'a':
				x--;
				x = (x < 0) ? 0 : x;
				break;
			case 's':
				y++;
				y = (y > 7) ? 7 : y;
				break;
			case 'd':
				x++;
				x = (x > 7) ? 7 : x;
				break;
			default:
				break;
			}
			curr_state = ST_DEFAULT;	// go to default state, re-print UI
			break;

		// State to select color
		case ST_SEL_COLOR:
			// Print select color interface
			UART_ESC_Code("[2J");						// clear terminal
			UART_ESC_Code("[H");						// move cursor to top left
			UART_ESC_Code("[5m");						// blinking text
			UART_ESC_Code("[1m");						// bold text
			UART_print("Select Color: \r\n");
			UART_ESC_Code("[0m");

			// Print keypress with respective color
			UART_print("R              Red\r\n");
			UART_print("O              Orange\r\n");
			UART_print("Y              Yellow\r\n");
			UART_print("G              Green\r\n");
			UART_print("B              Blue\r\n");
			UART_print("I              Indigo\r\n");
			UART_print("V              Violet\r\n");
			UART_print("P              Pink\r\n");
			UART_print("N              Brown\r\n");
			UART_print("W              White\r\n");

			while (!key_pressed_flag);				// wait for key to be pressed
			key_pressed_flag = 0;					// clear key press flag

			// Change color based on the key pressed
			switch (keypress) {
			case 'r':
				curr_color = RED;
				break;
			case 'o':
				curr_color = ORANGE;
				break;
			case 'y':
				curr_color = YELLOW;
				break;
			case 'g':
				curr_color = GREEN;
				break;
			case 'b':
				curr_color = BLUE;
				break;
			case 'i':
				curr_color = INDIGO;
				break;
			case 'v':
				curr_color = VIOLET;
				break;
			case 'p':
				curr_color = PINK;
				break;
			case 'n':
				curr_color = BROWN;
				break;
			case 'w':
				curr_color = WHITE;
				break;
			default:
				break;
			}
			curr_state = ST_DEFAULT;	// go to default state, re-print UI
			break;

		// State to draw pixel
		case ST_DRAW:
			Set_LED(LED_Matrix[y][x], curr_color.red, curr_color.green, curr_color.blue);	// Set LED using x, y, and current color
			Set_Brightness(DEFAULT_BRIGHTNESS);																// Set brightness of LED
			WS2812_Send();																	// Send LED data
			UI_Matrix[x][y] = curr_color.key;												// Change char on UI Grid to color keypress
			curr_state = ST_DEFAULT;														// go to default state, re-print UI
			break;

		// State to erase pixel
		case ST_ERASE:
			Set_LED(LED_Matrix[y][x], 0, 0, 0);		// Clear LED with (R, G, B) = (0, 0, 0)
			Set_Brightness(DEFAULT_BRIGHTNESS);		// Set brightness of LED (must be done for DMA to work)
			WS2812_Send();							// Send LED data
			UI_Matrix[x][y] = '*';					// Change char on UI Grid to '*'
			curr_state = ST_DEFAULT;				// go to default state, re-print UI
			break;

		// State to clear all LEDs
		case ST_CLEAR:
			Reset_LED();							// Reset LEDs
			Set_Brightness(DEFAULT_BRIGHTNESS);		// Set brightness (must be done for DMA to work)
			WS2812_Send();							// Send LED Data

			// Re-init UI Grid to '*' characters
			for (int i = 0; i < MATRIX_WIDTH; i++) {
				for (int j = 0; j < MATRIX_HEIGHT; j++) {
					UI_Matrix[i][j] = '*';
				}
			}

			curr_state = ST_DEFAULT;				// go to default state, re-print UI
			break;
		}
	}
	/* USER CODE END 3 */
}

/* prints the UI Grid and adjusts terminal cursor based on x and y coordinates */
void print_matrix(uint8_t x_pos, uint8_t y_pos) {
	// Print asterisks
	for (int i = 0; i < MATRIX_WIDTH; i++) {
		for (int j = 0; j < MATRIX_HEIGHT; j++) {
			UART_print_char(UI_Matrix[j][i]);
			UART_print_char(' ');
		}
		UART_print("\r\n");
	}

	UART_ESC_Code("[8A");						// Start cursor at top left of the grid

	// Move cursor x times to the right
	for (int m = 0; m < x_pos; m++) {
		UART_ESC_Code("[2C");					// Move cursor 2 positions right (to account for spaces between asterisks)
	}

	// Move cursor y times down
	for (int n = 0; n < y_pos; n++) {
		UART_ESC_Code("[1B");					// Move cursor 1 position down
	}
}

/* Interrupt handler for USART2 */
void USART2_IRQHandler(void) {
	// If read data register not empty, store the data (keypress) in global variable and set key press flag high
	if ((USART2->ISR & USART_ISR_RXNE) != 0) {
		keypress = USART2->RDR;
		key_pressed_flag = 1;
	}

}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1)
			!= HAL_OK) {
		Error_Handler();
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 9;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 0;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = 90 - 1;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2)
			!= HAL_OK) {
		Error_Handler();
	}
	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.BreakFilter = 0;
	sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
	sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
	sBreakDeadTimeConfig.Break2Filter = 0;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */
	HAL_TIM_MspPostInit(&htim1);

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel3_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
