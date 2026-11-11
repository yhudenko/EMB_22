#include "stm32f4xx_hal.h"
#include <string.h>

// Black Pill STM32F411CEU6: LED on PC13 (active LOW)
// USART1: PA9 = TX, PA10 = RX (to ST-Link VCP), 115200 baud
// Clock: default HSI 16 MHz

static UART_HandleTypeDef huart1;

static void gpio_init(void) {
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitTypeDef g = {0};
  g.Pin = GPIO_PIN_13;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &g);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);  // LED off
}

static void uart_init(void) {
  // Enable clocks for GPIOA and USART1
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  // Configure PA9 (TX) and PA10 (RX) as USART1 alternate function
  GPIO_InitTypeDef g = {0};
  g.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  g.Mode = GPIO_MODE_AF_PP;
  g.Pull = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &g);

  // UART settings: 115200 baud, 8N1, no flow control
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16; // for find signal middle
  HAL_UART_Init(&huart1);
}

static void print(const char *s) {
  HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

void SysTick_Handler(void) {
  HAL_IncTick();
}

int main(void) {
  HAL_Init();
  gpio_init();
  uart_init();

  while (1) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);  // LED on
    print("Hello from STM32F411! LED ON\r\n");
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    // LED off
    print("Hello from STM32F411! LED OFF\r\n");
    HAL_Delay(500);
  }
}
