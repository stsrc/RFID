#ifndef _RTC_H_
#define _RTC_H_
#include <stm32f1xx.h>
#include <stm32f1xx_hal_rcc.h>
#include <stm32f1xx_hal_gpio.h>
#include <stm32f1xx_hal_rtc.h>
#include <stm32f1xx_hal_cortex.h>

#include "tm_stm32f1_ili9341.h"
#include "simple_buffer.h"
#include "UART.h"
#include "RFID.h"


/**
 * @addtogroup RFID_System libraries
 * @{
 */

/**
 * @defgroup RTC
 * @brief RTC library.
 * @{
 *
 * This library allows to calculate date and time from 00:00:00 01.01.2016.
 * It also supports leap year.
 */


extern __IO uint8_t RTC_second_flag;

HAL_StatusTypeDef RTC_Init();

/**
 * @brief funtion returns actual time by pointer
 *
 * @retval uint8_t 0 if RTC_GetTime was called before new second has ticked.
 * 1 otherwise.
 */
uint8_t RTC_GetTime(uint8_t *hour, uint8_t *min, uint8_t *sec);

/**
 * @brief function sets time, passed by arguments
 */
void RTC_SetTime(uint8_t hour, uint8_t min, uint8_t sec);

/**
 * @brief function sets date, passed by arguments
 */
void RTC_SetDate(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, 
		 uint8_t min, uint8_t sec);

/**
 * @brief function retrieves actual time, returns it by pointers
 *
 * @retval uint8_t 0 if RTC_GetDate was called before new single second has elapsed.
 * 1 otherwise.
 */
uint8_t RTC_GetDate(uint16_t *year, uint8_t *month, uint8_t *day, 
		    uint8_t *hour, uint8_t *min, uint8_t *sec);

#endif
