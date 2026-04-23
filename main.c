#include "main.h"
#include "cmsis_os.h"
#include "gpio.h"
#include "usart.h"
#include <stdio.h>

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Queue for events
QueueHandle_t eventQueue;

// Event types
typedef enum {
    EVENT_MOTION,
    EVENT_NO_MOTION,
    EVENT_BUTTON
} EventType;

// Redirect printf to UART2
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

// ---------------- TASKS ----------------

void MotionTask(void *argument)
{
    GPIO_PinState lastState = GPIO_PIN_RESET;

    for (;;)
    {
        GPIO_PinState state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);

        if (state != lastState)
        {
            lastState = state;

            EventType evt = (state == GPIO_PIN_SET) ? EVENT_MOTION : EVENT_NO_MOTION;
            xQueueSend(eventQueue, &evt, 0);

            if (evt == EVENT_MOTION)
                printf("[MotionTask] Motion detected\r\n");
            else
                printf("[MotionTask] No motion\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void HVAC_Task(void *argument)
{
    EventType evt;

    for (;;)
    {
        if (xQueueReceive(eventQueue, &evt, portMAX_DELAY) == pdTRUE)
        {
            if (evt == EVENT_MOTION)
            {
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);   // A/C ON
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET); // Heater OFF
                printf("[HVAC] A/C ON, Heater OFF\r\n");
            }
            else if (evt == EVENT_NO_MOTION)
            {
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET); // A/C OFF
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);   // Heater ON
                printf("[HVAC] Heater ON, A/C OFF\r\n");
            }
            else if (evt == EVENT_BUTTON)
            {
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_6);
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_7);
                printf("[HVAC] Button pressed — toggling outputs\r\n");
            }
        }
    }
}

void ButtonTask(void *argument)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------- INTERRUPT CALLBACK ----------------

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (GPIO_Pin == GPIO_PIN_13)
    {
        EventType evt = EVENT_BUTTON;
        xQueueSendFromISR(eventQueue, &evt, &xHigherPriorityTaskWoken);
        printf("[ISR] Button interrupt\r\n");
    }

    if (GPIO_Pin == GPIO_PIN_1)
    {
        EventType evt = EVENT_MOTION;
        xQueueSendFromISR(eventQueue, &evt, &xHigherPriorityTaskWoken);
        printf("[ISR] Motion interrupt\r\n");
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ---------------- MAIN ----------------

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();   // UART2 from usart.c

    printf("\r\n=== System Boot ===\r\n");
    printf("FreeRTOS HVAC Controller Starting...\r\n");

    eventQueue = xQueueCreate(10, sizeof(EventType));

    xTaskCreate(MotionTask, "Motion", 128, NULL, 2, NULL);
    xTaskCreate(HVAC_Task, "HVAC", 128, NULL, 3, NULL);
    xTaskCreate(ButtonTask, "Button", 128, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {}
}

// ---------------- CLOCK CONFIG ----------------

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

// ---------------- ERROR HANDLER ----------------

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
