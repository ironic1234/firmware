#include "cooling.h"

Cooling_t cooling = {0};
volatile uint16_t raw_dt_flow_ct;
volatile uint16_t raw_bat_flow_ct;
uint32_t last_flow_meas_time_ms;
uint32_t dt_pump_start_time_ms;
uint32_t bat_pump_start_time_ms;

static void setDtCooling(uint8_t on);
static void setBatCooling(uint8_t on);
uint8_t lowpass(uint8_t new, uint8_t *old, uint8_t curr);

bool coolingInit()
{
    /* Configure GPIO Interrupts */
    // enable syscfg clock
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    // set exti gpio source through syscfg (0b0010 means GPIOC)
    SYSCFG->EXTICR[0] |= (((uint16_t)0b0010) << (DT_FLOW_RATE_PWM_Pin  & 0b11) * 4) |
                         (((uint16_t)0b0010) << (BAT_FLOW_RATE_PWM_Pin & 0b11) * 4);
    // unmask the interrupt
    EXTI->IMR1 |= (0b1 << DT_FLOW_RATE_PWM_Pin) | 
                  (0b1 << BAT_FLOW_RATE_PWM_Pin);
    // set trigger to rising edge
    EXTI->RTSR1 |= (0b1 << DT_FLOW_RATE_PWM_Pin) |
                   (0b1 << BAT_FLOW_RATE_PWM_Pin);
    // enable the interrupt handlers
    NVIC_EnableIRQ(EXTI1_IRQn);
    NVIC_EnableIRQ(EXTI2_IRQn);

    // Default pin configurations
    setDtCooling(0);
    setBatCooling(0);

    return true;
}

uint8_t lowpass(uint8_t new, uint8_t *old, uint8_t curr) {
    uint8_t i;
    float   average = 0;

    old[curr] = new;    

    for (i = 0; i < AVG_WINDOW_SIZE; i++) {
        average += (float) old[i];
    }

    return (uint8_t) (average / AVG_WINDOW_SIZE);
}

void coolingPeriodic()
{
    /* WATER TEMP CALCULATIONS */

    // cooling.dt_therm_1_C    = rawThermtoCelcius(adc_readings.dt_therm_1);
    // cooling.dt_therm_2_C    = rawThermtoCelcius(adc_readings.dt_therm_2);
    // cooling.bat_therm_in_C  = rawThermtoCelcius(adc_readings.bat_therm_in);
    // cooling.bat_therm_out_C = rawThermtoCelcius(adc_readings.bat_therm_out);

    /* FLOW CALCULATIONS */
    // Convert ticks and time delta to liters per minute
    cooling.dt_liters_p_min_x10 = ((1000 / (float) (cooling.dt_delta_t * 7.5))) * 10;
    cooling.bat_liters_p_min_x10 = ((1000 / (float) (cooling.bat_delta_t * 7.5))) * 10;
    
    static uint8_t dt_old[AVG_WINDOW_SIZE];
    static uint8_t bat_old[AVG_WINDOW_SIZE];
    static uint8_t curr;
    cooling.dt_liters_p_min_x10 = lowpass(cooling.dt_liters_p_min_x10, dt_old, curr);
    cooling.bat_liters_p_min_x10 = lowpass(cooling.bat_liters_p_min_x10, bat_old, curr);
    ++curr;
    curr = (curr == AVG_WINDOW_SIZE) ? 0 : curr;

    /* DT COOLANT SYSTEM */

    // Find max motor temperature (CELSIUS)
    uint8_t max_motor_temp = MAX(can_data.front_motor_currents_temps.left_temp,
                                 can_data.front_motor_currents_temps.right_temp);
    max_motor_temp = MAX(max_motor_temp, can_data.rear_motor_currents_temps.left_temp);
    max_motor_temp = MAX(max_motor_temp, can_data.rear_motor_currents_temps.right_temp);

    // Determine if temp error
    cooling.dt_temp_error = can_data.front_motor_currents_temps.stale ||
                            can_data.rear_motor_currents_temps.stale  ||
                            max_motor_temp >= DT_ERROR_TEMP_C;
    // Check flow rate
    if (cooling.dt_pump && !cooling.dt_rose && 
        ((sched.os_ticks - dt_pump_start_time_ms) / 1000) > DT_FLOW_STARTUP_TIME_S)
        cooling.dt_rose = 1;
    if (cooling.dt_pump && cooling.dt_rose && 
        cooling.dt_liters_p_min_x10 < DT_MIN_FLOW_L_M * 10)
    {
        cooling.dt_flow_error = 1;
    }
    else
    {
        // TODO: how to reset error?
        //cooling.dt_flow_error = 0;
    }

    max_motor_temp = 0;
    // Determine if system should be on
    if ((!cooling.dt_flow_error || DT_FLOW_CHECK_OVERRIDE) && 
    (max_motor_temp > DT_PUMP_ON_TEMP_C || ((PHAL_readGPIO(PRCHG_STAT_GPIO_Port, PRCHG_STAT_Pin)) &&
    (cooling.dt_temp_error || DT_ALWAYS_COOL))))
    {
        if (!cooling.dt_pump)
        {
            setDtCooling(true);
        }
    }
    // Determine if system should be off
    else if (cooling.dt_pump)
    {
        setDtCooling(false);
    }

    /* BAT COOLANT SYSTEM */

    // TODO: replace with CAN frame
    uint8_t max_bat_temp = 0;

    cooling.bat_temp_error = 1||// TODO: replace with CAN frame can_data.bat_temp.stale ||
                             max_bat_temp >= BAT_ERROR_TEMP_C;

    // Check flow rate
    if (cooling.bat_pump && !cooling.bat_rose && 
        ((sched.os_ticks - bat_pump_start_time_ms) / 1000) > BAT_FLOW_STARTUP_TIME_S)
        cooling.bat_rose = 1;
    if (cooling.bat_pump && cooling.bat_rose && 
        cooling.bat_liters_p_min_x10 < BAT_MIN_FLOW_L_M * 10)
    {
        cooling.bat_flow_error = 1;
    }
    else
    {
        // TODO: how to reset error?
        //cooling.bat_flow_error = 0;
    }

    max_bat_temp = 0;
    // Determine if system should be on
    if ((!cooling.bat_flow_error || BAT_FLOW_CHECK_OVERRIDE) && 
    (max_bat_temp > BAT_PUMP_ON_TEMP_C || ((PHAL_readGPIO(PRCHG_STAT_GPIO_Port, PRCHG_STAT_Pin)) &&
    (cooling.bat_temp_error || BAT_ALWAYS_COOL))))
    {
        if (!cooling.bat_pump)
        {
            setBatCooling(true);
        }
    }
    // Determine if system should be off
    else if (cooling.bat_pump)
    {
        setBatCooling(false);
    }    

}

void setFanPWM(void) {
    uint8_t set_dt = 0;
    uint8_t set_bat = 0;
    static uint16_t time_curr;

    if (time_curr < cooling.dt_fan_power) {
        set_dt = 1;
    }

    if (time_curr < cooling.bat_fan_power) {
        set_bat = 1;
    }

    if (++time_curr == 10) {
        time_curr = 0;
    }

    PHAL_writeGPIO(DT_RAD_FAN_CTRL_GPIO_Port, DT_RAD_FAN_CTRL_Pin, set_dt);
    PHAL_writeGPIO(BAT_RAD_FAN_CTRL_GPIO_Port, BAT_RAD_FAN_CTRL_Pin, set_bat);
}

void setDtCooling(uint8_t on)
{
    if (!cooling.dt_pump && on) dt_pump_start_time_ms = sched.os_ticks;
    if (!on) cooling.dt_rose = 0;
    cooling.dt_pump = on;
    PHAL_writeGPIO(DT_PUMP_CTRL_GPIO_Port, DT_PUMP_CTRL_Pin, on);
    cooling.dt_fan_power = on ? 4 : 0;
    // PHAL_writeGPIO(DT_RAD_FAN_CTRL_GPIO_Port, DT_RAD_FAN_CTRL_Pin, on);
}

void setBatCooling(uint8_t on)
{
    if (!cooling.bat_pump && on) bat_pump_start_time_ms = sched.os_ticks;
    if (!on) cooling.bat_rose = 0;
    cooling.bat_pump = on;
    PHAL_writeGPIO(BAT_PUMP_CTRL_GPIO_Port, BAT_PUMP_CTRL_Pin, on);
    cooling.bat_fan_power = on ? 4 : 0;
    // PHAL_writeGPIO(BAT_RAD_FAN_CTRL_GPIO_Port, BAT_RAD_FAN_CTRL_Pin, on);
}

float rawThermtoCelcius(uint16_t t)
{
    float resistance;
    resistance = (t == MAX_THERM) ? FLT_MAX : THERM_R1 * t / (MAX_THERM - t);
    return (resistance > 0) ? THERM_A * log(resistance) + THERM_B : 0;
}


/* Interrupt handlers for counting sensor ticks */

void EXTI1_IRQHandler()
{
    static uint32_t last_flow_meas_time_ms;

    // check pin responsible
    if (EXTI->PR1 & (0b1 << DT_FLOW_RATE_PWM_Pin))
    {
        cooling.dt_delta_t = sched.os_ticks - last_flow_meas_time_ms;
        last_flow_meas_time_ms = sched.os_ticks;

        // clear flag
        EXTI->PR1 = (0b1 << DT_FLOW_RATE_PWM_Pin);
    }
}

void EXTI2_IRQHandler()
{
    static uint32_t last_flow_meas_time_ms;

    // check pin responsible
    if (EXTI->PR1 & (0b1 << BAT_FLOW_RATE_PWM_Pin))
    {
        cooling.bat_delta_t = sched.os_ticks - last_flow_meas_time_ms;
        last_flow_meas_time_ms = sched.os_ticks;

        // clear flag
        EXTI->PR1 = (0b1 << BAT_FLOW_RATE_PWM_Pin);
    }
}