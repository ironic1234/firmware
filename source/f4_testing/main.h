#include "cmsis_os2.h"

#ifndef _MAIN_H_
#define _MAIN_H_

#define RED 14
#define BLUE 15
#define GREEN 12
#define ORANGE 13

typedef struct
{
    osThreadAttr_t orange; // Attributes for the orange LED thread
    osThreadAttr_t green;  // Attributes for the green LED thread
    osThreadAttr_t red;    // Attributes for the red LED thread
    osThreadAttr_t blue;   // Attributes for the blue LED thread
} LedThreadAttrs_t;

#endif
