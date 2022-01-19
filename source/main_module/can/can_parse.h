/**
 * @file can_parse.h
 * @author Luke Oxley (lcoxley@purdue.edu)
 * @brief Parsing of CAN messages using auto-generated structures with bit-fields
 * @version 0.1
 * @date 2021-09-15
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef _CAN_PARSE_H_
#define _CAN_PARSE_H_

#include "common/queue/queue.h"
#include "common/phal_L4/can/can.h"

// Make this match the node name within the can_config.json
#define NODE_NAME "Main_Module"

#define RX_UPDATE_PERIOD 15 // ms

// Message ID definitions
/* BEGIN AUTO ID DEFS */
#define ID_DRIVER_REQUEST 0x4000040
#define ID_MAIN_STATUS 0x4001900
#define ID_RAW_THROTTLE_BRAKE 0x14000285
#define ID_START_BUTTON 0x4000005
/* END AUTO ID DEFS */

// Message DLC definitions
/* BEGIN AUTO DLC DEFS */
#define DLC_DRIVER_REQUEST 4
#define DLC_MAIN_STATUS 3
#define DLC_RAW_THROTTLE_BRAKE 8
#define DLC_START_BUTTON 1
/* END AUTO DLC DEFS */

// Message sending macros
/* BEGIN AUTO SEND MACROS */
#define SEND_DRIVER_REQUEST(queue, accel_, regen_) do {\
        CanMsgTypeDef_t msg = {.Bus=CAN1, .ExtId=ID_DRIVER_REQUEST, .DLC=DLC_DRIVER_REQUEST, .IDE=1};\
        CanParsedData_t* data_a = (CanParsedData_t *) &msg.Data;\
        data_a->driver_request.accel = accel_;\
        data_a->driver_request.regen = regen_;\
        qSendToBack(&queue, &msg);\
    } while(0)
#define SEND_MAIN_STATUS(queue, car_state_, apps_state_, precharge_state_) do {\
        CanMsgTypeDef_t msg = {.Bus=CAN1, .ExtId=ID_MAIN_STATUS, .DLC=DLC_MAIN_STATUS, .IDE=1};\
        CanParsedData_t* data_a = (CanParsedData_t *) &msg.Data;\
        data_a->main_status.car_state = car_state_;\
        data_a->main_status.apps_state = apps_state_;\
        data_a->main_status.precharge_state = precharge_state_;\
        qSendToBack(&queue, &msg);\
    } while(0)
/* END AUTO SEND MACROS */

// Stale Checking
#define STALE_THRESH 3 / 2 // 3 / 2 would be 150% of period
/* BEGIN AUTO UP DEFS (Update Period)*/
#define UP_RAW_THROTTLE_BRAKE 5
/* END AUTO UP DEFS */

#define CHECK_STALE(stale, curr, last, period) if(!stale && \
                    (curr - last) * RX_UPDATE_PERIOD > period * STALE_THRESH) stale = 1

// Message Raw Structures
/* BEGIN AUTO MESSAGE STRUCTURE */
typedef union { __attribute__((packed))
    struct {
        uint64_t accel: 16;
        uint64_t regen: 16;
    } driver_request;
    struct {
        uint64_t car_state: 8;
        uint64_t apps_state: 8;
        uint64_t precharge_state: 1;
    } main_status;
    struct {
        uint64_t throttle0: 16;
        uint64_t throttle1: 16;
        uint64_t brake0: 16;
        uint64_t brake1: 16;
    } raw_throttle_brake;
    struct {
        uint64_t start: 1;
    } start_button;
    uint8_t raw_data[8];
} CanParsedData_t;
/* END AUTO MESSAGE STRUCTURE */

// contains most up to date received
// type for each variable matches that defined in JSON
/* BEGIN AUTO CAN DATA STRUCTURE */
typedef struct {
    struct {
        uint16_t throttle0;
        uint16_t throttle1;
        uint16_t brake0;
        uint16_t brake1;
        uint8_t stale;
        uint32_t last_rx;
    } raw_throttle_brake;
    struct {
        uint16_t start;
    } start_button;
} can_data_t;
/* END AUTO CAN DATA STRUCTURE */

extern can_data_t can_data;

/* BEGIN AUTO EXTERN CALLBACK */
extern void start_button_CALLBACK(CanParsedData_t* msg_data_a);
/* END AUTO EXTERN CALLBACK */

/* BEGIN AUTO EXTERN RX IRQ */
/* END AUTO EXTERN RX IRQ */

/**
 * @brief Setup queue and message filtering
 * 
 * @param q_rx_can RX buffer of CAN messages
 */
void initCANParse(q_handle_t* q_rx_can_a);

/**
 * @brief Pull message off of rx buffer,
 *        update can_data struct,
 *        check for stale messages
 */
void canRxUpdate();

/**
 * @brief Process any rx message callbacks from the CAN Rx IRQ
 * 
 * @param rx rx data from message just recieved
 */
void canProcessRxIRQs(CanMsgTypeDef_t* rx);

#endif