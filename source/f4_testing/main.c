#include "cmsis_os2.h"
#include "common/phal_F4_F7/gpio/gpio.h"
#include "common/phal_F4_F7/rcc/rcc.h"

#include "main.h"

GPIOInitConfig_t gpio_config[] = {
    GPIO_INIT_OUTPUT(GPIOD, ORANGE, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(GPIOD, GREEN, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(GPIOD, BLUE, GPIO_OUTPUT_LOW_SPEED),
    GPIO_INIT_OUTPUT(GPIOD, RED, GPIO_OUTPUT_LOW_SPEED),

};

#define TargetCoreClockrateHz 16000000
ClockRateConfig_t clock_config = {
    .system_source = SYSTEM_CLOCK_SRC_HSI,
    .vco_output_rate_target_hz = 160000000,
    .system_clock_target_hz = TargetCoreClockrateHz,
    .ahb_clock_target_hz = (TargetCoreClockrateHz / 1),
    .apb1_clock_target_hz = (TargetCoreClockrateHz / (1)),
    .apb2_clock_target_hz = (TargetCoreClockrateHz / (1)),
};

extern uint32_t APB1ClockRateHz;
extern uint32_t APB2ClockRateHz;
extern uint32_t AHBClockRateHz;
extern uint32_t PLLClockRateHz;

void HardFault_Handler();

void orange_led_blink();
void green_led_blink();
void blue_led_blink();
void red_led_blink();

volatile uint32_t delay = 1000;

int main()
{
    osKernelInitialize();

    if (0 != PHAL_configureClockRates(&clock_config))
    {
        HardFault_Handler();
    }
    if (!PHAL_initGPIO(gpio_config,
                       sizeof(gpio_config) / sizeof(GPIOInitConfig_t)))
    {
        HardFault_Handler();
    }

    /* Task Creation */
    osThreadNew(orange_led_blink, NULL, NULL);
    osThreadNew(green_led_blink, NULL, NULL);
    osThreadNew(red_led_blink, NULL, NULL);
    osThreadNew(blue_led_blink, NULL, NULL);
    /* Schedule Periodic tasks here */

    osKernelStart();
    return 0;
}

void orange_led_blink()
{
    while (1)
    {
        PHAL_toggleGPIO(GPIOD, ORANGE);
        osDelay(delay);
    }
}

void green_led_blink()
{
    while (1)
    {
        PHAL_toggleGPIO(GPIOD, GREEN);
        osDelay(delay / 2);
    }
}

void blue_led_blink()
{
    while (1)
    {
        PHAL_toggleGPIO(GPIOD, BLUE);
        osDelay(delay * 1.5);
    }
}

void red_led_blink()
{
    while (1)
    {
        PHAL_toggleGPIO(GPIOD, RED);
        osDelay(delay * 2);
    }
}

void HardFault_Handler()
{
    while (1)
    {
        __asm__("nop");
    }
}
