#include "car.h"

volatile Car_t car;
extern q_handle_t q_tx_can;
uint32_t buzzer_start_tick = 0;
volatile ADCReadings_t adc_readings;
uint8_t prchg_set;

static bool checkErrorFaults();
static bool checkFatalFaults();
static void brakeLightUpdate(uint16_t raw_brake);

inline float pow_diy(float base, uint8_t power)
{
    float output = base;
    for(uint8_t i = 0; i < (power - 1); i++)
    {
        output *= base;
    }
    return output;
}

bool carInit()
{
    car.state = CAR_STATE_INIT;
    PHAL_writeGPIO(SDC_CTRL_GPIO_Port, SDC_CTRL_Pin, 0);
    PHAL_writeGPIO(BUZZER_GPIO_Port, BUZZER_Pin, 0);
    prchg_set = 0;
}

void carHeartbeat()
{
    SEND_MAIN_HB(q_tx_can, car.state, prchg_set);
}

/**
 * @brief Main task cor the car containing a finite state machine
 *        to determine when the car should be driveable
 */
uint32_t last_time = 0;
uint8_t curr = 0;
void carPeriodic()
{

    torqueRequest_t torque_r;
    static uint16_t t_temp;

    /* State Independent Operations */

    // TODO: brakeLightUpdate(can_data.raw_throttle_brake.brake);
    if (can_data.raw_throttle_brake.brake > BRAKE_LIGHT_ON_THRESHOLD)
    {
        if (!car.brake_light)
        {
            PHAL_writeGPIO(BRK_LIGHT_GPIO_Port, BRK_LIGHT_Pin, true);
            car.brake_light = true;
        }
    }
    else if (car.brake_light)
    {
        PHAL_writeGPIO(BRK_LIGHT_GPIO_Port, BRK_LIGHT_Pin, false);
        car.brake_light = false;
    }


    if (checkErrorFaults())
    {
        car.state = CAR_STATE_ERROR;
    }
    // A fatal fault has higher prority
    // than an error fault
    if (checkFatalFaults())
    {
        car.state = CAR_STATE_FATAL;
    }

    /* State Dependent Operations */

    // EV.10.4 - Activation sequence
    // Tractive System Active - SDC closed, HV outside accumulator
    // Ready to drive - motors respond to APPS input
    //                  not possible unless:
    //                   - tractive system active
    //                   - brake pedal pressed and held
    //                   - start button press

    if (car.state == CAR_STATE_FATAL)
    {
        // SDC critical error has occured, open sdc
        PHAL_writeGPIO(SDC_CTRL_GPIO_Port, SDC_CTRL_Pin, false); // open SDC
    }
    else if (car.state == CAR_STATE_ERROR)
    {
        // Error has occured, leave HV on but do not drive
        PHAL_writeGPIO(SDC_CTRL_GPIO_Port, SDC_CTRL_Pin, true); // close SDC
        // Recover once error gone
        if (!checkErrorFaults()) car.state = CAR_STATE_INIT;
    }
    else if (car.state == CAR_STATE_INIT)
    {
        PHAL_writeGPIO(SDC_CTRL_GPIO_Port, SDC_CTRL_Pin, true); // close SDC
        if (can_data.start_button.start)
        {
            can_data.start_button.start = 0; // debounce
            car.state = CAR_STATE_BUZZING;
        }
    }
    else if (car.state == CAR_STATE_BUZZING)
    {
        // EV.10.5 - Ready to drive sound
        // 1-3 seconds, unique from other sounds
        if (!PHAL_readGPIO(BUZZER_GPIO_Port, BUZZER_Pin))
        {
            PHAL_writeGPIO(BUZZER_GPIO_Port, BUZZER_Pin, true);
            buzzer_start_tick = sched.os_ticks;
        }
        // stop buzzer
        else if (sched.os_ticks - buzzer_start_tick > BUZZER_DURATION_MS)
        {
            PHAL_writeGPIO(BUZZER_GPIO_Port, BUZZER_Pin, false);
            car.state = CAR_STATE_READY2DRIVE;
        }
    }
    else if (car.state == CAR_STATE_READY2DRIVE)
    {
        // Check if requesting to exit ready2drive
        if (can_data.start_button.start)
        {
            can_data.start_button.start = 0; // debounce
            car.state = CAR_STATE_INIT;
        }

        // Send torque command to all 2 motors

        // ?? how about set the deadzone on can_data.raw_throttle_brake.throttle 

        // int16_t t_req = can_data.raw_throttle_brake.throttle - ((can_data.raw_throttle_brake.brake > 409) ? can_data.raw_throttle_brake.brake : 0);
        // t_req = t_req < 100 ? 0 : ((t_req - 100) / (4095 - 100) * 4095);

        uint16_t adjusted_throttle = (can_data.raw_throttle_brake.throttle < 100) ? 0 : (can_data.raw_throttle_brake.throttle - 100) * 4095 / (4095 - 100);
        
        int16_t t_req = adjusted_throttle - ((can_data.raw_throttle_brake.brake > 409) ? can_data.raw_throttle_brake.brake : 0); 

        // SEND_TORQUE_REQUEST_MAIN(q_tx_can, t_req, t_req, t_req, t_req);
        if (sched.os_ticks - last_time > 2000){ 
            //curr++;
            curr = !curr;
            last_time = sched.os_ticks;
        }

        // t_temp = (t_temp > 469) ? 0 : t_temp + 1;

        // E-diff
        eDiff(t_req, &torque_r);

        // check torque request (FSAE rule)
        if(torque_r.torque_left > t_req)
        {
            torque_r.torque_left = t_req;
        }
        if(torque_r.torque_right > t_req)
        {
            torque_r.torque_right = t_req;
        }

        SEND_TORQUE_REQUEST_MAIN(q_tx_can, 0, 0, torque_r.torque_left, torque_r.torque_right);

        /************ Around the World *************/
        // Meant for testing with vehicle off the ground
        // Periodically cycles each wheel in a circular pattern
        // if (curr) SEND_TORQUE_REQUEST_MAIN(q_tx_can, 0, 0, 0, 410);
        // else SEND_TORQUE_REQUEST_MAIN(q_tx_can, 0, 0, 0, 0);
        /*
        switch(curr)
        {
            case 0:
            SEND_TORQUE_REQUEST_MAIN(q_tx_can, t_req, 0, 0, 0);//t_req, t_req, t_req);
            break;
            case 1:
            SEND_TORQUE_REQUEST_MAIN(q_tx_can, 0, t_req, 0, 0);//t_req, t_req, t_req);
            break;
            case 2:
            SEND_TORQUE_REQUEST_MAIN(q_tx_can, 0, 0, t_req, 0);//t_req, t_req, t_req);
            break;
            case 3:
            SEND_TORQUE_REQUEST_MAIN(q_tx_can, 0, 0, 0, t_req);//t_req, t_req, t_req);
            break;
        }*/

    }
}

/**
 * @brief  Checks faults that should prevent
 *         the car from driving, but are okay
 *         to leave the sdc closed
 * 
 * @return true  Faults exist
 * @return false No faults have existed for set time
 */
uint32_t last_error_time = 0;
bool error_rose = 0;
bool checkErrorFaults()
{
    uint8_t is_error = 0;
    uint8_t prchg_stat;
    static uint16_t prchg_time;

    /* Heart Beat Stale */ 
    // is_error += can_data.dashboard_hb.stale;
    // is_error += can_data.front_driveline_hb.stale;
    // TODO: is_error += can_data.rear_driveline_hb.stale;
    // TODO: is_error += can_data.precharge_hb.stale;

    prchg_stat = PHAL_readGPIO(PRCHG_STAT_GPIO_Port, PRCHG_STAT_Pin);

    if (!prchg_stat) {
        ++prchg_time;
    } else {
        prchg_set = 1;
        prchg_time = 0;
    }

    if (prchg_time > (500 / 15)) {
        --prchg_time;
        ++is_error;
        prchg_set = 0;
    }

    /* Precharge */
    // TODO: is_error += !PHAL_readGPIO(PRCHG_STAT_GPIO_Port, PRCHG_STAT_Pin);

    /* Dashboard */
    // TODO: is_error += can_data.raw_throttle_brake.stale;

    /* Driveline */
    // Front
    // is_error += can_data.front_driveline_hb.front_left_motor  != 
    //             FRONT_LEFT_MOTOR_CONNECTED;
    // is_error += can_data.front_driveline_hb.front_right_motor != 
    //             FRONT_RIGHT_MOTOR_CONNECTED;

    // Rear
    is_error += can_data.rear_driveline_hb.rear_left_motor    != 
                REAR_LEFT_MOTOR_CONNECTED;
    is_error += can_data.rear_driveline_hb.rear_right_motor   != 
                REAR_RIGHT_MOTOR_CONNECTED;

    /* Temperature */
    // TODO: if (!DT_ALWAYS_COOL)  is_error += cooling.dt_temp_error;
    // TODO: if (!BAT_ALWAYS_COOL) is_error += cooling.bat_temp_error;

    if (is_error && !error_rose) 
    {
        error_rose = true;
        last_error_time = sched.os_ticks;
    }

    if (!is_error && error_rose &&
        sched.os_ticks - last_error_time > ERROR_FALL_MS)
    {
        error_rose = false;
    }

    return is_error;
}

/**
 * @brief  Checks faults that should open the SDC
 * @return true  Faults exist
 * @return false No faults have existed for set time
 */
bool checkFatalFaults()
{
    uint8_t is_error = 0;

    if (!DT_FLOW_CHECK_OVERRIDE)  is_error += cooling.dt_flow_error;
    if (!BAT_FLOW_CHECK_OVERRIDE) is_error += cooling.bat_flow_error;

    // TODO: is_error += !PHAL_readGPIO(LIPO_BAT_STAT_GPIO_Port, LIPO_BAT_STAT_Pin);

    is_error += (can_data.max_cell_temp.max_temp > 500) ? 1 : 0;

    return is_error;
}

/**
 * @brief Calculates the low voltage system current
 *        draw based on the output of a current
 *        sense amplifier
 */
void calcLVCurrent()
{
    uint32_t raw = adc_readings.lv_i_sense;
    car.lv_current_mA = (uint16_t) (raw * 1000 * 1000 * LV_ADC_V_IN_V / 
                        (LV_MAX_ADC_RAW * LV_GAIN * LV_R_SENSE_mOHM));
}

static void brakeLightUpdate(uint16_t raw_brake)
{
    if (!car.brake_light)
    {
        if (raw_brake >= BRAKE_LIGHT_ON_THRESHOLD)
        {
            car.brake_light = 1;
            car.brake_blink_ct = 0;
            car.brake_start_time = sched.os_ticks;
            PHAL_writeGPIO(BRK_LIGHT_GPIO_Port, BRK_LIGHT_Pin, true);
        }
    }
    else
    {
        if (raw_brake < BRAKE_LIGHT_OFF_THRESHOLD)
        {
            car.brake_light = 0;
            PHAL_writeGPIO(BRK_LIGHT_GPIO_Port, BRK_LIGHT_Pin, false);
        }
        else if (car.brake_blink_ct != BRAKE_BLINK_CT)
        {
            if (sched.os_ticks - car.brake_start_time > BRAKE_BLINK_PERIOD / 2)
            {
                PHAL_toggleGPIO(BRK_LIGHT_GPIO_Port, BRK_LIGHT_Pin);
                if (PHAL_readGPIO(BRK_LIGHT_GPIO_Port, BRK_LIGHT_Pin)) ++car.brake_blink_ct;
                car.brake_start_time = sched.os_ticks;
            }
        }
    }
}

/*
* E-diff @ May 29, based on Demetrius's May 25 E-diff simulink model
*/

void eDiff(int16_t t_req, torqueRequest_t* torque_r)
{
    float steering_wheel_angle;

    float wheel_speed_left;
    float wheel_speed_right;
    float vehicle_speed;

    float rack_disp;
    float left_steer_angle;
    float right_steer_angle;
    float avg_steer_angle;

    float desired_wheel_speed_left;
    float desired_wheel_speed_right;
    float desired_wheel_speed_diff;

    float actual_wheel_speed_diff;
    float error_wheel_speed_diff;

    float delta_torque;

    if (t_req < 0) // when regen breaking
    {
        torque_r->torque_left = t_req;
        torque_r->torque_right = t_req;

        return;
    } 
    else // when positive torque request
    {
        wheel_speed_left = can_data.rear_wheel_data.left_speed / 100.0;
        wheel_speed_right = can_data.rear_wheel_data.right_speed / 100.0;

        // not the best vehicle speed calculation, should probably include IMU data also
        vehicle_speed = (wheel_speed_left + wheel_speed_right) / 2.0;

        steering_wheel_angle = can_data.LWS_Standard.LWS_ANGLE / 10.0;
        rack_disp = STEERING_SLOPE * STEERING_INCH2MM * steering_wheel_angle;

        left_steer_angle = - ((STEERING_S1 * pow_diy(-rack_disp, 3)) + (STEERING_S2 * pow_diy(-rack_disp, 2)) + (STEERING_S3 * (-rack_disp)) + STEERING_S4);
        right_steer_angle = ((STEERING_S1 * pow_diy(rack_disp, 3)) + (STEERING_S2 * pow_diy(rack_disp, 2)) + (STEERING_S3 * rack_disp) + STEERING_S4);
        avg_steer_angle = ((left_steer_angle + right_steer_angle) / 2) * STEERING_DEG2RAD;

        // E-diff equations
        desired_wheel_speed_left = vehicle_speed + vehicle_speed * STEERING_W * tan(avg_steer_angle) / (2 * STEERING_L);
        desired_wheel_speed_right = vehicle_speed - vehicle_speed * STEERING_W * tan(avg_steer_angle) / (2 * STEERING_L);

        // pre-PID
        desired_wheel_speed_diff = (desired_wheel_speed_left - desired_wheel_speed_right) * STEERING_K;

        actual_wheel_speed_diff = wheel_speed_left - wheel_speed_right;

        error_wheel_speed_diff = desired_wheel_speed_diff - actual_wheel_speed_diff;

        // actual PID
        delta_torque = fabs(STEERING_P * error_wheel_speed_diff);  // just P for now
        
        //  TODO (not critical): include lookup table for torque fallout in high rpm

        // torque output
        if(error_wheel_speed_diff >= 0)
        {
            torque_r->torque_left = t_req;
            torque_r->torque_right = t_req - delta_torque;
            // limit regen torque (temporary)
            if(torque_r->torque_right < (- 0.25 * 4095))
            {
                torque_r->torque_right = (- 0.25 * 4095);
            }
        }
        else
        {
            torque_r->torque_left = t_req - delta_torque;
            torque_r->torque_right = t_req;
            // limit regen torque (temporary)
            if(torque_r->torque_left < (- 0.25 * 4095))
            {
                torque_r->torque_left = (- 0.25 * 4095);
            }
        }

        return;
    }
}