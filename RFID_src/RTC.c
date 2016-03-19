#include "RTC.h"

__IO uint8_t RTC_cnt = 0;

void RTC_IRQHandler() {
	RTC_cnt++;
	if (RTC_cnt == 1) {
		HAL_NVIC_EnableIRQ(EXTI3_IRQn);
	} else if (RTC_cnt == 31) {
		TM_ILI9341_DisplayOff();
		RTC_cnt = 0;
	}
	RTC->CRL &= ~RTC_CRL_SECF;
}

HAL_StatusTypeDef RTC_Init()
{
	/*
	 * After reset access to RTC registers is deisable, so
	 * there is need to update RCC register. Look to datasheet
	 */
	RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
	PWR->CR |= PWR_CR_DBP;
	// Setting clock to RTC. Using LSE clock (low speed external)
	RCC->BDCR |= RCC_BDCR_RTCSEL_0;
	RCC->BDCR &= RCC_BDCR_RTCSEL_1;
	// Enabling LSE oscillator
	RCC->BDCR |= RCC_BDCR_LSEON;
	// Waiting for stable oscillator
	while(!(RCC->BDCR & RCC_BDCR_LSERDY));
	// Enabling RTC clock;
	RCC->BDCR |= RCC_BDCR_RTCEN;	
	/* Synchronizing APB1 bus with RTC (read datasheet)*/
	while(!(RTC->CRL & RTC_CRL_RSF));
	/* Waiting for end of write operation */
	while(!(RTC->CRL & RTC_CRL_RTOFF));
	/* Entering configuration mode */
	RTC->CRL |= RTC_CRL_CNF;

	/*Writing to registers*/
	
	/* Second interrupt enable */
	RTC->CRH = RTC_CRH_SECIE;
	/* RTC prescaler reload value high */
	RTC->PRLH = 0;
	/* RTC prescaler reload value low */
	RTC->PRLL = 0x7FFF;
	
	/* Leaving configuration mode */
	RTC->CRL &= ~RTC_CRL_CNF;
	/* Waiting for end of write operation */
	while(!(RTC->CRL & RTC_CRL_RTOFF));
	HAL_NVIC_SetPriority(RTC_IRQn, 2, 0); 
	HAL_NVIC_EnableIRQ(RTC_IRQn);
	return HAL_OK;
}
