/**************************************************************************
*
* Copyright (C) 2007 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/
#ifndef LOADCONTROL_H
#define LOADCONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "bacnet/bacnet_stack_exports.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacerror.h"
#include "bacnet/rp.h"
#include "bacnet/wp.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


    /* load control objects are required to support LEVEL */
    typedef enum BACnetShedLevelType {
        BACNET_SHED_TYPE_PERCENT, /* Unsigned */
        BACNET_SHED_TYPE_LEVEL, /* Unsigned */
        BACNET_SHED_TYPE_AMOUNT /* REAL */
    } BACNET_SHED_LEVEL_TYPE;


    #define DEFAULT_VALUE_PERCENT 100
    #define DEFAULT_VALUE_LEVEL 0
    #define DEFAULT_VALUE_AMOUNT 0

    /* The shed levels for the LEVEL choice of BACnetShedLevel
   that have meaning for this particular Load Control object. */
    typedef struct {
        BACNET_SHED_LEVEL_TYPE type;
        union {
            unsigned level;
            unsigned percent;
            float amount;
        } value;
    } BACNET_SHED_LEVEL;

    typedef enum load_control_state {
        SHED_INACTIVE,
        SHED_REQUEST_PENDING,
        SHED_NON_COMPLIANT,
        SHED_COMPLIANT,
        MAX_LOAD_CONTROL_STATE
    } LOAD_CONTROL_STATE;


    #define MAX_SHED_LEVELS 3


    typedef struct load_control_descr
    {
        BACNET_SHED_STATE Present_Value;
        /* indicates the desired load shedding */
        BACNET_SHED_LEVEL Requested_Shed_Level;
        /* Indicates the amount of power that the object expects
        to be able to shed in response to a load shed request. */
        BACNET_SHED_LEVEL Expected_Shed_Level;
        /* Indicates the actual amount of power being shed in response
        to a load shed request. */
        BACNET_SHED_LEVEL Actual_Shed_Level;
        /* indicates the start of the duty window in which the load controlled
        by the Load Control object must be compliant with the requested shed. */
        BACNET_DATE_TIME Start_Time;
        BACNET_DATE_TIME End_Time;
        /* indicates the duration of the load shed action,
        starting at Start_Time in minutes */
        uint32_t Shed_Duration;
        /* indicates the time window used for load shed accounting in minutes */
        uint32_t Duty_Window;
        /* indicates and controls whether the Load Control object is
        currently enabled to respond to load shed requests.  */
        bool Load_Control_Enable;
        /* indicates when the object receives a write to any of the properties
        Requested_Shed_Level, Shed_Duration, Duty_Window */
        bool Load_Control_Request_Written;
        /* indicates when the object receives a write to Start_Time */
        bool Start_Time_Property_Written;
        /* optional: indicates the baseline power consumption value
        for the sheddable load controlled by this object,
        if a fixed baseline is used.
        The units of Full_Duty_Baseline are kilowatts.*/
        float Full_Duty_Baseline;
                /* Represents the shed levels for the LEVEL choice of
        BACnetShedLevel that have meaning for this particular
        Load Control object. */
        /* The elements of the array are required to be writable,
        allowing local configuration of how this Load Control
        object will participate in load shedding for the
        facility. This array is not required to be resizable
        through BACnet write services. The size of this array
        shall be equal to the size of the Shed_Level_Descriptions
        array. The behavior of this object when the Shed_Levels
        array contains duplicate entries is a local matter. */
        unsigned Shed_Levels[MAX_SHED_LEVELS];
        /* represents a description of the shed levels that the
        Load Control object can take on.  It is the same for
        all the load control objects in this example device. */
        char *Shed_Level_Descriptions[MAX_SHED_LEVELS];
        float Shed_Level_Values[MAX_SHED_LEVELS];

        LOAD_CONTROL_STATE Load_Control_State;
        LOAD_CONTROL_STATE Load_Control_State_Previously;
        char *Object_Name;
        char *Description;
    } LOAD_CONTROL_DESCR;

    BACNET_STACK_EXPORT
    void Load_Control_Property_Lists(
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);
    BACNET_STACK_EXPORT
    void Load_Control_State_Machine_Handler(
        void);

    BACNET_STACK_EXPORT
    bool Load_Control_Valid_Instance(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    unsigned Load_Control_Count(
        void);
    BACNET_STACK_EXPORT
    uint32_t Load_Control_Index_To_Instance(
        unsigned index);
    BACNET_STACK_EXPORT
    unsigned Load_Control_Instance_To_Index(
        uint32_t object_instance);

    BACNET_STACK_EXPORT
    bool Load_Control_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);
    BACNET_STACK_EXPORT
    bool Load_Control_Name_Set(uint32_t object_instance, char *new_name);
    BACNET_STACK_EXPORT
    bool Load_Control_Description(uint32_t instance,
        BACNET_CHARACTER_STRING *object_descr);
    BACNET_STACK_EXPORT
    bool Load_Control_Description_Set(uint32_t instance, char *new_name);

    BACNET_STACK_EXPORT
    void Load_Control_Resize(size_t new_size);
    BACNET_STACK_EXPORT
    void Load_Control_Add(size_t count);
    BACNET_STACK_EXPORT
    void Load_Control_Alloc(size_t new_size);
    BACNET_STACK_EXPORT
    void Load_Control_Free(void);
    BACNET_STACK_EXPORT
    void Load_Control_Objects_Init(void);    

    BACNET_STACK_EXPORT
    void Load_Control_Init(void);
    BACNET_STACK_EXPORT
    void Load_Control_Cleanup(void);

    BACNET_STACK_EXPORT
    void Load_Control_State_Machine(
        int object_index);

    BACNET_STACK_EXPORT
    int Load_Control_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata);

    BACNET_STACK_EXPORT
    bool Load_Control_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data);

#ifdef BAC_TEST
#include "ctest.h"
    BACNET_STACK_EXPORT
    void testLoadControl(
        Test * pTest);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
