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

/* Load Control Objects - customize for your use */
/* from 135-2004-Addendum e */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memcpy */
#include <pthread.h>
#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/datetime.h"
#include "bacnet/bacenum.h"
#include "bacnet/config.h" /* the custom stuff */
#include "bacnet/basic/object/lc.h"
#include "bacnet/basic/object/ao.h"
#include "bacnet/wp.h"
#include "bacnet/basic/services.h"



/*  indicates the current load shedding state of the object */
static LOAD_CONTROL_DESCR *Load_Control_Descr = NULL;
static size_t Load_Control_Descr_Size = 0;
static pthread_mutex_t LC_Descr_Mutex = PTHREAD_MUTEX_INITIALIZER;

static BACNET_DATE_TIME Current_Time;

/* represents a description of the shed levels that the
   Load Control object can take on.  It is the same for
   all the load control objects in this example device. */
static char *Shed_Level_Descriptions[MAX_SHED_LEVELS] = { "dim lights 10%",
    "dim lights 20%", "dim lights 30%" };

static float Shed_Level_Values[MAX_SHED_LEVELS] = { 90.0, 80.0, 70.0 };

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Load_Control_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE, PROP_STATUS_FLAGS,
    PROP_EVENT_STATE, PROP_REQUESTED_SHED_LEVEL, PROP_START_TIME,
    PROP_SHED_DURATION, PROP_DUTY_WINDOW, PROP_ENABLE, PROP_EXPECTED_SHED_LEVEL,
    PROP_ACTUAL_SHED_LEVEL, PROP_SHED_LEVELS, PROP_SHED_LEVEL_DESCRIPTIONS,
    -1 };

static const int Load_Control_Properties_Optional[] = { PROP_DESCRIPTION,
    PROP_FULL_DUTY_BASELINE, -1 };

static const int Load_Control_Properties_Proprietary[] = { -1 };

void Load_Control_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Load_Control_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Load_Control_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Load_Control_Properties_Proprietary;
    }

    return;
}

void Load_Control_Resize(size_t new_size)
{
    Load_Control_Free();
    Load_Control_Alloc(new_size);
    Load_Control_Objects_Init();
}

void Load_Control_Add(size_t count)
{
    size_t new_size = Load_Control_Descr_Size + count;

    pthread_mutex_lock(&LC_Descr_Mutex);
    LOAD_CONTROL_DESCR *tmp = realloc(Load_Control_Descr, sizeof(*Load_Control_Descr) * new_size);
    if (NULL == tmp) {
        pthread_mutex_unlock(&LC_Descr_Mutex);
        return;
    }
    Load_Control_Descr_Size = new_size;
    Load_Control_Descr = tmp;
    pthread_mutex_unlock(&LC_Descr_Mutex);

    Load_Control_Objects_Init();
}

void Load_Control_Free(void)
{
    if (NULL == Load_Control_Descr) return;

    pthread_mutex_lock(&LC_Descr_Mutex);
    for (unsigned i = 0; i < Load_Control_Descr_Size; i++) {
        free(Load_Control_Descr[i].Object_Name);
        free(Load_Control_Descr[i].Description);
    }
    free(Load_Control_Descr);
    Load_Control_Descr = NULL;
    Load_Control_Descr_Size = 0;
    pthread_mutex_unlock(&LC_Descr_Mutex);
}

void Load_Control_Alloc(size_t size)
{
    Load_Control_Descr = calloc(size, sizeof (*Load_Control_Descr));
    if (NULL != Load_Control_Descr)
    {
        Load_Control_Descr_Size = size;
    }
}

void Load_Control_Objects_Init()
{
    unsigned i, j;

    for (i = 0; i < Load_Control_Descr_Size; i++) {
        /* FIXME: load saved data? */
        Load_Control_Descr[i].Present_Value = BACNET_SHED_INACTIVE;
        Load_Control_Descr[i].Requested_Shed_Level.type = BACNET_SHED_TYPE_LEVEL;
        Load_Control_Descr[i].Requested_Shed_Level.value.level = 0;
        datetime_wildcard_set(&Load_Control_Descr[i].Start_Time);
        datetime_wildcard_set(&Load_Control_Descr[i].End_Time);
        datetime_wildcard_set(&Current_Time);
        Load_Control_Descr[i].Shed_Duration = 0;
        Load_Control_Descr[i].Duty_Window = 0;
        Load_Control_Descr[i].Load_Control_Enable = true;
        Load_Control_Descr[i].Full_Duty_Baseline = 1.500; /* kilowatts */
        Load_Control_Descr[i].Expected_Shed_Level.type = BACNET_SHED_TYPE_LEVEL;
        Load_Control_Descr[i].Expected_Shed_Level.value.level = 0;
        Load_Control_Descr[i].Actual_Shed_Level.type = BACNET_SHED_TYPE_LEVEL;
        Load_Control_Descr[i].Actual_Shed_Level.value.level = 0;
        Load_Control_Descr[i].Load_Control_Request_Written = false;
        Load_Control_Descr[i].Start_Time_Property_Written = false;
        for (j = 0; j < MAX_SHED_LEVELS; j++) {
            Load_Control_Descr[i].Shed_Levels[j] = j + 1;
        }
        Load_Control_Descr[i].Object_Name = NULL;
        Load_Control_Descr[i].Description = NULL;
    }

    return;
}

void Load_Control_Cleanup()
{
    Load_Control_Free();
}

void Load_Control_Init(void)
{
    
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool Load_Control_Valid_Instance(uint32_t object_instance)
{
    if (object_instance < Load_Control_Descr_Size) {
        return true;
    }

    return false;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Load_Control_Count(void)
{
    return Load_Control_Descr_Size;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Load_Control_Index_To_Instance(unsigned index)
{
    return index;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Load_Control_Instance_To_Index(uint32_t object_instance)
{
    unsigned index = Load_Control_Descr_Size;

    if (object_instance < Load_Control_Descr_Size) {
        index = object_instance;
    }

    return index;
}

static BACNET_SHED_STATE Load_Control_Present_Value(uint32_t object_instance)
{
    BACNET_SHED_STATE value = BACNET_SHED_INACTIVE;
    unsigned index = 0;

    index = Load_Control_Instance_To_Index(object_instance);
    if (index < Load_Control_Descr_Size) {
        value = Load_Control_Descr[index].Present_Value ;
    }

    return value;
}

/* note: the object name must be unique within this device */
bool Load_Control_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    static char text_string[32] = ""; /* okay for single thread */
    unsigned index;
    bool status = false;

    index = Load_Control_Instance_To_Index(object_instance);
    if (index >= Load_Control_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&LC_Descr_Mutex);
    if (NULL != Load_Control_Descr[index].Object_Name) {
        snprintf(text_string, 32, "%s", Load_Control_Descr[index].Object_Name);
    } else {
        sprintf(text_string, "LOAD CONTROL %u", object_instance);
    }
    pthread_mutex_unlock(&LC_Descr_Mutex);

    status = characterstring_init_ansi(object_name, text_string);

    return status;
}

bool Load_Control_Name_Set(uint32_t object_instance, char *new_name)
{
    unsigned index = Load_Control_Instance_To_Index(object_instance);
    if (index >= Load_Control_Descr_Size) return false;

    pthread_mutex_lock(&LC_Descr_Mutex);
    free(Load_Control_Descr[index].Object_Name);
    Load_Control_Descr[index].Object_Name = calloc(strlen(new_name) + 1, sizeof(char));
    if (Load_Control_Descr[index].Object_Name) {
        strcpy(Load_Control_Descr[index].Object_Name, new_name);
    }
    pthread_mutex_unlock(&LC_Descr_Mutex);
    return true;
}

bool Load_Control_Description(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_descr)
{
    static char text_string[32] = "";
    unsigned index;
    bool status = false;

    index = Load_Control_Instance_To_Index(object_instance);
    if (index >= Load_Control_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&LC_Descr_Mutex);
    if (NULL != Load_Control_Descr[index].Description) {
        snprintf(text_string, 32, "%s", Load_Control_Descr[index].Description);
    } else {
        sprintf(text_string, "LOAD CONTROL %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&LC_Descr_Mutex);

    status = characterstring_init_ansi(object_descr, text_string);

    return status;
}

bool Load_Control_Description_Set(uint32_t object_instance, char *new_descr)
{
    unsigned index = Load_Control_Instance_To_Index(object_instance);
    if (index >= Load_Control_Descr_Size) return false;

    pthread_mutex_lock(&LC_Descr_Mutex);
    free(Load_Control_Descr[index].Description);
    Load_Control_Descr[index].Description = calloc(strlen(new_descr) + 1, sizeof(char));
    if (Load_Control_Descr[index].Description) {
        strcpy(Load_Control_Descr[index].Description, new_descr);
    }
    pthread_mutex_unlock(&LC_Descr_Mutex);
    return true;
}

static void Update_Current_Time(BACNET_DATE_TIME *bdatetime)
{
    if (bdatetime) {
        datetime_local(&bdatetime->date, &bdatetime->time, NULL, NULL);
    }
}

/* convert the shed level request into an Analog Output Present_Value */
static float Requested_Shed_Level_Value(int object_index)
{
    unsigned shed_level_index = 0;
    unsigned i = 0;
    float requested_level = 0.0;

    switch (Load_Control_Descr[object_index].Requested_Shed_Level.type) {
        case BACNET_SHED_TYPE_PERCENT:
            requested_level =
                (float)Load_Control_Descr[object_index].Requested_Shed_Level.value.percent;
            break;
        case BACNET_SHED_TYPE_AMOUNT:
            /* Assumptions: wattage is linear with analog output level */
            requested_level = Load_Control_Descr[object_index].Full_Duty_Baseline -
                Load_Control_Descr[object_index].Requested_Shed_Level.value.amount;
            requested_level /= Load_Control_Descr[object_index].Full_Duty_Baseline;
            requested_level *= 100.0;
            break;
        case BACNET_SHED_TYPE_LEVEL:
        default:
            for (i = 0; i < MAX_SHED_LEVELS; i++) {
                if (Load_Control_Descr[object_index].Shed_Levels[i] <=
                    Load_Control_Descr[object_index].Requested_Shed_Level.value.level) {
                    shed_level_index = i;
                }
            }
            requested_level = Shed_Level_Values[shed_level_index];
            break;
    }

    return requested_level;
}

static void Shed_Level_Copy(BACNET_SHED_LEVEL *dest, BACNET_SHED_LEVEL *src)
{
    if (dest && src) {
        dest->type = src->type;
        switch (src->type) {
            case BACNET_SHED_TYPE_PERCENT:
                dest->value.percent = src->value.percent;
                break;
            case BACNET_SHED_TYPE_AMOUNT:
                dest->value.amount = src->value.amount;
                break;
            case BACNET_SHED_TYPE_LEVEL:
            default:
                dest->value.level = src->value.level;
                break;
        }
    }
}

static void Shed_Level_Default_Set(
    BACNET_SHED_LEVEL *dest, BACNET_SHED_LEVEL_TYPE type)
{
    if (dest) {
        dest->type = type;
        switch (type) {
            case BACNET_SHED_TYPE_PERCENT:
                dest->value.percent = 100;
                break;
            case BACNET_SHED_TYPE_AMOUNT:
                dest->value.amount = 0.0;
                break;
            case BACNET_SHED_TYPE_LEVEL:
            default:
                dest->value.level = 0;
                break;
        }
    }
}

static bool Able_To_Meet_Shed_Request(int object_index)
{
    float level = 0.0;
    float requested_level = 0.0;
    unsigned priority = 0;
    bool status = false;
    int object_instance = 0;

    /* This demo is going to use the Analog Outputs as their Load */
    object_instance = object_index;
    priority = Analog_Output_Present_Value_Priority(object_instance);
    /* we are controlling at Priority 4 - can we control the output? */
    if (priority >= 4) {
        /* is the level able to be lowered? */
        requested_level = Requested_Shed_Level_Value(object_index);
        level = Analog_Output_Present_Value(object_instance);
        if (level >= requested_level) {
            status = true;
        }
    }

    return status;
}

#if PRINT_ENABLED_DEBUG
static void Print_Load_Control_State(int object_index)
{
    char *Load_Control_State_Text[MAX_LOAD_CONTROLS] = { "SHED_INACTIVE",
        "SHED_REQUEST_PENDING", "SHED_NON_COMPLIANT", "SHED_COMPLIANT" };

    if (object_index < Load_Control_Descr_Size) {
        if (MAX_LOAD_CONTROLSLoad_Control_State[object_index] < MAX_LOAD_CONTROL_STATE) {
            printf("Load Control[%d]=%s\n", object_index,
                Load_Control_State_Text[Load_Control_State[object_index]]);
        }
    }
}
#endif

void Load_Control_State_Machine(int object_index)
{
    unsigned i = 0; /* loop counter */
    int diff = 0; /* used for datetime comparison */

    /* is the state machine enabled? */
    if (!Load_Control_Descr[object_index].Load_Control_Enable) {
        Load_Control_Descr[object_index].Load_Control_State = SHED_INACTIVE;
        return;
    }

    switch (Load_Control_Descr[object_index].Load_Control_State) {
        case SHED_REQUEST_PENDING:
            if (Load_Control_Descr[object_index].Load_Control_Request_Written) {
                Load_Control_Descr[object_index].Load_Control_Request_Written = false;
                /* request to cancel using default values? */
                switch (Load_Control_Descr[object_index].Requested_Shed_Level.type) {
                    case BACNET_SHED_TYPE_PERCENT:
                        if (Load_Control_Descr[object_index].Requested_Shed_Level.value.percent ==
                            DEFAULT_VALUE_PERCENT) {
                            Load_Control_Descr[object_index].Load_Control_State = SHED_INACTIVE;
                        }
                        break;
                    case BACNET_SHED_TYPE_AMOUNT:
                        if (Load_Control_Descr[object_index].Requested_Shed_Level.value.amount ==
                            DEFAULT_VALUE_AMOUNT) {
                            Load_Control_Descr[object_index].Load_Control_State = SHED_INACTIVE;
                        }
                        break;
                    case BACNET_SHED_TYPE_LEVEL:
                    default:
                        if (Load_Control_Descr[object_index].Requested_Shed_Level.value.level ==
                            DEFAULT_VALUE_LEVEL) {
                            Load_Control_Descr[object_index].Load_Control_State = SHED_INACTIVE;
                        }
                        break;
                }
                if (Load_Control_Descr[object_index].Load_Control_State == SHED_INACTIVE) {
#if PRINT_ENABLED_DEBUG
                    printf("Load Control[%d]:Requested Shed Level=Default\n",
                        object_index);
#endif
                    break;
                }
            }
            /* clear the flag for Start time if it is written */
            if (Load_Control_Descr[object_index].Start_Time_Property_Written) {
                Load_Control_Descr[object_index].Start_Time_Property_Written = false;
                /* request to cancel using wildcards in start time? */
                if (datetime_wildcard(&Load_Control_Descr[object_index].Start_Time)) {
                    Load_Control_Descr[object_index].Load_Control_State = SHED_INACTIVE;
#if PRINT_ENABLED_DEBUG
                    printf(
                        "Load Control[%d]:Start Time=Wildcard\n", object_index);
#endif
                    break;
                }
            }
            /* cancel because current time is after start time + duration? */
            datetime_copy(&Load_Control_Descr[object_index].End_Time, &Load_Control_Descr[object_index].Start_Time);
            datetime_add_minutes(
                &Load_Control_Descr[object_index].End_Time, Load_Control_Descr[object_index].Shed_Duration);
            diff = datetime_compare(&Load_Control_Descr[object_index].End_Time, &Current_Time);
            if (diff < 0) {
                /* CancelShed */
                /* FIXME: stop shedding! i.e. relinquish */
#if PRINT_ENABLED_DEBUG
                printf("Load Control[%d]:Current Time"
                       " is after Start Time + Duration\n",
                    object_index);
#endif
                Load_Control_Descr[object_index].Load_Control_State = SHED_INACTIVE;
                break;
            }
            diff = datetime_compare(&Current_Time, &Load_Control_Descr[object_index].Start_Time);
            if (diff < 0) {
                /* current time prior to start time */
                /* ReconfigurePending */
                Shed_Level_Copy(&Load_Control_Descr[object_index].Expected_Shed_Level,
                    &Load_Control_Descr[object_index].Requested_Shed_Level);
                Shed_Level_Default_Set(&Load_Control_Descr[object_index].Actual_Shed_Level,
                    Load_Control_Descr[object_index].Requested_Shed_Level.type);
            } else if (diff > 0) {
                /* current time after to start time */
#if PRINT_ENABLED_DEBUG
                printf("Load Control[%d]:Current Time"
                       " is after Start Time\n",
                    object_index);
#endif
                /* AbleToMeetShed */
                if (Able_To_Meet_Shed_Request(object_index)) {
                    Shed_Level_Copy(&Load_Control_Descr[object_index].Expected_Shed_Level,
                        &Load_Control_Descr[object_index].Requested_Shed_Level);
                    Analog_Output_Present_Value_Set(object_index,
                        Requested_Shed_Level_Value(object_index), 4);
                    Shed_Level_Copy(&Load_Control_Descr[object_index].Actual_Shed_Level,
                        &Load_Control_Descr[object_index].Requested_Shed_Level);
                    Load_Control_Descr[object_index].Load_Control_State = SHED_COMPLIANT;
                } else {
                    /* CannotMeetShed */
                    Shed_Level_Default_Set(&Load_Control_Descr[object_index].Expected_Shed_Level,
                        Load_Control_Descr[object_index].Requested_Shed_Level.type);
                    Shed_Level_Default_Set(&Load_Control_Descr[object_index].Actual_Shed_Level,
                        Load_Control_Descr[object_index].Requested_Shed_Level.type);
                    Load_Control_Descr[object_index].Load_Control_State = SHED_NON_COMPLIANT;
                }
            }
            break;
        case SHED_NON_COMPLIANT:
            datetime_copy(&Load_Control_Descr[object_index].End_Time, &Load_Control_Descr[object_index].Start_Time);
            datetime_add_minutes(
                &Load_Control_Descr[object_index].End_Time, Load_Control_Descr[object_index].Shed_Duration);
            diff = datetime_compare(&Load_Control_Descr[object_index].End_Time, &Current_Time);
            if (diff < 0) {
                /* FinishedUnsuccessfulShed */
#if PRINT_ENABLED_DEBUG
                printf("Load Control[%d]:Current Time is after Start Time + "
                       "Duration\n",
                    object_index);
#endif
                Load_Control_Descr[object_index].Load_Control_State = SHED_INACTIVE;
                break;
            }
            if (Load_Control_Descr[object_index].Load_Control_Request_Written ||
                Load_Control_Descr[object_index].Start_Time_Property_Written) {
                /* UnsuccessfulShedReconfigured */
#if PRINT_ENABLED_DEBUG
                printf("Load Control[%d]:Control Property written\n",
                    object_index);
#endif
                /* The Written flags will cleared in the next state */
                Load_Control_Descr[object_index].Load_Control_State = SHED_REQUEST_PENDING;
                break;
            }
            if (Able_To_Meet_Shed_Request(object_index)) {
                /* CanNowComplyWithShed */
#if PRINT_ENABLED_DEBUG
                printf("Load Control[%d]:Able to meet Shed Request\n",
                    object_index);
#endif
                Shed_Level_Copy(&Load_Control_Descr[object_index].Expected_Shed_Level,
                    &Load_Control_Descr[object_index].Requested_Shed_Level);
                Analog_Output_Present_Value_Set(
                    object_index, Requested_Shed_Level_Value(object_index), 4);
                Shed_Level_Copy(&Load_Control_Descr[object_index].Actual_Shed_Level,
                    &Load_Control_Descr[object_index].Requested_Shed_Level);
                Load_Control_Descr[object_index].Load_Control_State = SHED_COMPLIANT;
            }
            break;
        case SHED_COMPLIANT:
            datetime_copy(&Load_Control_Descr[object_index].End_Time, &Load_Control_Descr[object_index].Start_Time);
            datetime_add_minutes(
                &Load_Control_Descr[object_index].End_Time, Load_Control_Descr[object_index].Shed_Duration);
            diff = datetime_compare(&Load_Control_Descr[object_index].End_Time, &Current_Time);
            if (diff < 0) {
                /* FinishedSuccessfulShed */
#if PRINT_ENABLED_DEBUG
                printf("Load Control[%d]:Current Time is after Start Time + "
                       "Duration\n",
                    object_index);
#endif
                datetime_wildcard_set(&Load_Control_Descr[i].Start_Time);
                Analog_Output_Present_Value_Relinquish(object_index, 4);
                Load_Control_Descr[object_index].Load_Control_State = SHED_INACTIVE;
                break;
            }
            if (Load_Control_Descr[object_index].Load_Control_Request_Written ||
                Load_Control_Descr[object_index].Start_Time_Property_Written) {
                /* UnsuccessfulShedReconfigured */
#if PRINT_ENABLED_DEBUG
                printf("Load Control[%d]:Control Property written\n",
                    object_index);
#endif
                /* The Written flags will cleared in the next state */
                Load_Control_Descr[object_index].Load_Control_State = SHED_REQUEST_PENDING;
                break;
            }
            if (!Able_To_Meet_Shed_Request(object_index)) {
                /* CanNoLongerComplyWithShed */
#if PRINT_ENABLED_DEBUG
                printf("Load Control[%d]:Not able to meet Shed Request\n",
                    object_index);
#endif
                Shed_Level_Default_Set(&Load_Control_Descr[object_index].Expected_Shed_Level,
                    Load_Control_Descr[object_index].Requested_Shed_Level.type);
                Shed_Level_Default_Set(&Load_Control_Descr[object_index].Actual_Shed_Level,
                    Load_Control_Descr[object_index].Requested_Shed_Level.type);
                Load_Control_Descr[object_index].Load_Control_State = SHED_NON_COMPLIANT;
            }
            break;
        case SHED_INACTIVE:
        default:
            if (Load_Control_Descr[object_index].Start_Time_Property_Written) {
#if PRINT_ENABLED_DEBUG
                printf("Load Control[%d]:Start Time written\n", object_index);
#endif
                /* The Written flag will cleared in the next state */
                Shed_Level_Copy(&Load_Control_Descr[object_index].Expected_Shed_Level,
                    &Load_Control_Descr[object_index].Requested_Shed_Level);
                Shed_Level_Default_Set(&Load_Control_Descr[object_index].Actual_Shed_Level,
                    Load_Control_Descr[object_index].Requested_Shed_Level.type);
                Load_Control_Descr[object_index].Load_Control_State = SHED_REQUEST_PENDING;
            }
            break;
    }

    return;
}

/* call every second or so */
void Load_Control_State_Machine_Handler(void)
{
    unsigned i = 0;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        for (i = 0; i < Load_Control_Descr_Size; i++) {
            Load_Control_Descr[i].Load_Control_State = SHED_INACTIVE;
            Load_Control_Descr[i].Load_Control_State_Previously = SHED_INACTIVE;
        }
    }
    Update_Current_Time(&Current_Time);
    for (i = 0; i < Load_Control_Descr_Size; i++) {
        Load_Control_State_Machine(i);
        if (Load_Control_Descr[i].Load_Control_State != Load_Control_Descr[i].Load_Control_State_Previously) {
#if PRINT_ENABLED_DEBUG
            Print_Load_Control_State(i);
#endif
            Load_Control_Descr[i].Load_Control_State_Previously = Load_Control_Descr[i].Load_Control_State;
        }
    }
}

/* return apdu len, or BACNET_STATUS_ERROR on error */
int Load_Control_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int len = 0;
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    int enumeration = 0;
    unsigned object_index = 0;
    unsigned i = 0;
    bool state = false;
    uint8_t *apdu = NULL;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;
    object_index = Load_Control_Instance_To_Index(rpdata->object_instance);
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_LOAD_CONTROL, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Load_Control_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_DESCRIPTION:
            Load_Control_Description(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], OBJECT_LOAD_CONTROL);
            break;
        case PROP_PRESENT_VALUE:
            enumeration = Load_Control_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], enumeration);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
            /* IN_ALARM - Logical FALSE (0) if the Event_State property
               has a value of NORMAL, otherwise logical TRUE (1). */
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            /* FAULT - Logical      TRUE (1) if the Reliability property is
               present and does not have a value of NO_FAULT_DETECTED,
               otherwise logical FALSE (0). */
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            /* OVERRIDDEN - Logical TRUE (1) if the point has been
               overridden by some mechanism local to the BACnet Device,
               otherwise logical FALSE (0). */
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            /* OUT_OF_SERVICE - This bit shall always be Logical FALSE (0). */
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, false);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_STATE:
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
            break;
        case PROP_REQUESTED_SHED_LEVEL:
            switch (Load_Control_Descr[object_index].Requested_Shed_Level.type) {
                case BACNET_SHED_TYPE_PERCENT:
                    apdu_len = encode_context_unsigned(&apdu[0], 0,
                        Load_Control_Descr[object_index].Requested_Shed_Level.value.percent);
                    break;
                case BACNET_SHED_TYPE_AMOUNT:
                    apdu_len = encode_context_real(&apdu[0], 2,
                        Load_Control_Descr[object_index].Requested_Shed_Level.value.amount);
                    break;
                case BACNET_SHED_TYPE_LEVEL:
                default:
                    apdu_len = encode_context_unsigned(&apdu[0], 1,
                        Load_Control_Descr[object_index].Requested_Shed_Level.value.level);
                    break;
            }
            break;
        case PROP_START_TIME:
            len = encode_application_date(
                &apdu[0], &Load_Control_Descr[object_index].Start_Time.date);
            apdu_len = len;
            len = encode_application_time(
                &apdu[apdu_len], &Load_Control_Descr[object_index].Start_Time.time);
            apdu_len += len;
            break;
        case PROP_SHED_DURATION:
            apdu_len = encode_application_unsigned(
                &apdu[0], Load_Control_Descr[object_index].Shed_Duration);
            break;
        case PROP_DUTY_WINDOW:
            apdu_len = encode_application_unsigned(
                &apdu[0], Load_Control_Descr[object_index].Duty_Window);
            break;
        case PROP_ENABLE:
            state = Load_Control_Descr[object_index].Load_Control_Enable;
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_FULL_DUTY_BASELINE: /* optional */
            apdu_len = encode_application_real(
                &apdu[0], Load_Control_Descr[object_index].Full_Duty_Baseline);
            break;
        case PROP_EXPECTED_SHED_LEVEL:
            switch (Load_Control_Descr[object_index].Expected_Shed_Level.type) {
                case BACNET_SHED_TYPE_PERCENT:
                    apdu_len = encode_context_unsigned(&apdu[0], 0,
                        Load_Control_Descr[object_index].Expected_Shed_Level.value.percent);
                    break;
                case BACNET_SHED_TYPE_AMOUNT:
                    apdu_len = encode_context_real(&apdu[0], 2,
                        Load_Control_Descr[object_index].Expected_Shed_Level.value.amount);
                    break;
                case BACNET_SHED_TYPE_LEVEL:
                default:
                    apdu_len = encode_context_unsigned(&apdu[0], 1,
                        Load_Control_Descr[object_index].Expected_Shed_Level.value.level);
                    break;
            }
            break;
        case PROP_ACTUAL_SHED_LEVEL:
            switch (Load_Control_Descr[object_index].Actual_Shed_Level.type) {
                case BACNET_SHED_TYPE_PERCENT:
                    apdu_len = encode_context_unsigned(&apdu[0], 0,
                        Load_Control_Descr[object_index].Actual_Shed_Level.value.percent);
                    break;
                case BACNET_SHED_TYPE_AMOUNT:
                    apdu_len = encode_context_real(&apdu[0], 2,
                        Load_Control_Descr[object_index].Actual_Shed_Level.value.amount);
                    break;
                case BACNET_SHED_TYPE_LEVEL:
                default:
                    apdu_len = encode_context_unsigned(&apdu[0], 1,
                        Load_Control_Descr[object_index].Actual_Shed_Level.value.level);
                    break;
            }
            break;
        case PROP_SHED_LEVELS:
            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0) {
                apdu_len =
                    encode_application_unsigned(&apdu[0], MAX_SHED_LEVELS);
                /* if no index was specified, then try to encode the entire list
                 */
                /* into one packet. */
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                apdu_len = 0;
                for (i = 0; i < MAX_SHED_LEVELS; i++) {
                    /* FIXME: check if we have room before adding it to APDU */
                    len = encode_application_unsigned(
                        &apdu[apdu_len], Load_Control_Descr[object_index].Shed_Levels[i]);
                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU) {
                        apdu_len += len;
                    } else {
                        rpdata->error_code =
                            ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
                        apdu_len = BACNET_STATUS_ABORT;
                        break;
                    }
                }
            } else {
                if (rpdata->array_index <= MAX_SHED_LEVELS) {
                    apdu_len = encode_application_unsigned(&apdu[0],
                        Load_Control_Descr[object_index].Shed_Levels[rpdata->array_index - 1]);
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;
        case PROP_SHED_LEVEL_DESCRIPTIONS:
            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0) {
                apdu_len =
                    encode_application_unsigned(&apdu[0], MAX_SHED_LEVELS);
                /* if no index was specified, then try to encode the entire list
                 */
                /* into one packet. */
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                apdu_len = 0;
                for (i = 0; i < MAX_SHED_LEVELS; i++) {
                    /* FIXME: check if we have room before adding it to APDU */
                    characterstring_init_ansi(
                        &char_string, Shed_Level_Descriptions[i]);
                    len = encode_application_character_string(
                        &apdu[apdu_len], &char_string);
                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU) {
                        apdu_len += len;
                    } else {
                        rpdata->error_code =
                            ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
                        apdu_len = BACNET_STATUS_ABORT;
                        break;
                    }
                }
            } else {
                if (rpdata->array_index <= MAX_SHED_LEVELS) {
                    characterstring_init_ansi(&char_string,
                        Shed_Level_Descriptions[rpdata->array_index - 1]);
                    apdu_len = encode_application_character_string(
                        &apdu[0], &char_string);
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) &&
        (rpdata->object_property != PROP_SHED_LEVEL_DESCRIPTIONS) &&
        (rpdata->object_property != PROP_SHED_LEVELS) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/* returns true if successful */
bool Load_Control_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    unsigned int object_index = 0;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_DATE
    TempDate; /* build here in case of error in time half of datetime */

    /* decode the some of the request */
    len = bacapp_decode_application_data(
        wp_data->application_data, wp_data->application_data_len, &value);
    /* FIXME: len < application_data_len: more data? */
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    /*  only array properties can have array options */
    if ((wp_data->object_property != PROP_SHED_LEVELS) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
    object_index = Load_Control_Instance_To_Index(wp_data->object_instance);
    switch (wp_data->object_property) {
        case PROP_REQUESTED_SHED_LEVEL:
            len = bacapp_decode_context_data(wp_data->application_data,
                wp_data->application_data_len, &value,
                PROP_REQUESTED_SHED_LEVEL);
            if (len == BACNET_STATUS_ERROR) {
                /* error! */
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            } else if (value.context_tag == 0) {
                /* percent - Unsigned */
                Load_Control_Descr[object_index].Requested_Shed_Level.type =
                    BACNET_SHED_TYPE_PERCENT;
                Load_Control_Descr[object_index].Requested_Shed_Level.value.percent =
                    value.type.Unsigned_Int;
                status = true;
            } else if (value.context_tag == 1) {
                /* level - Unsigned */
                Load_Control_Descr[object_index].Requested_Shed_Level.type =
                    BACNET_SHED_TYPE_LEVEL;
                Load_Control_Descr[object_index].Requested_Shed_Level.value.level =
                    value.type.Unsigned_Int;
                status = true;
            } else if (value.context_tag == 2) {
                /* amount - REAL */
                Load_Control_Descr[object_index].Requested_Shed_Level.type =
                    BACNET_SHED_TYPE_AMOUNT;
                Load_Control_Descr[object_index].Requested_Shed_Level.value.amount =
                    value.type.Real;
                status = true;
            } else {
                /* error! */
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            }
            if (status) {
                Load_Control_Descr[object_index].Load_Control_Request_Written = true;
            }
            break;

        case PROP_START_TIME:
            status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_DATE,
                &wp_data->error_class, &wp_data->error_code);
            if (!status) {
                /* don't continue if we don't have a valid date */
                break;
            }
            /* Hold the date until we are sure the time is also there */
            TempDate = value.type.Date;
            len =
                bacapp_decode_application_data(wp_data->application_data + len,
                    wp_data->application_data_len - len, &value);
            if (len) {
                status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_TIME,
                    &wp_data->error_class, &wp_data->error_code);
                if (status) {
                    /* Write time and date and set written flag */
                    Load_Control_Descr[object_index].Start_Time.date = TempDate;
                    Load_Control_Descr[object_index].Start_Time.time = value.type.Time;
                    Load_Control_Descr[object_index].Start_Time_Property_Written = true;
                }
            } else {
                status = false;
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
            }
            break;

        case PROP_SHED_DURATION:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                    &wp_data->error_class, &wp_data->error_code);
            if (status) {
                Load_Control_Descr[object_index].Shed_Duration = value.type.Unsigned_Int;
                Load_Control_Descr[object_index].Load_Control_Request_Written = true;
            }
            break;

        case PROP_DUTY_WINDOW:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                    &wp_data->error_class, &wp_data->error_code);
            if (status) {
                Load_Control_Descr[object_index].Duty_Window = value.type.Unsigned_Int;
                Load_Control_Descr[object_index].Load_Control_Request_Written = true;
            }
            break;

        case PROP_SHED_LEVELS:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                    &wp_data->error_class, &wp_data->error_code);
            if (status) {
                /* re-write the size of the array? */
                if (wp_data->array_index == 0) {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                    status = false;
                } else if (wp_data->array_index == BACNET_ARRAY_ALL) {
                    /* FIXME: write entire array */
                } else if (wp_data->array_index <= MAX_SHED_LEVELS) {
                    Load_Control_Descr[object_index].Shed_Levels[wp_data->array_index - 1] =
                        value.type.Unsigned_Int;
                } else {
                    /* FIXME: Something's missing from here so I'll just put in
                     * a place holder error here for the moment*/
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_OTHER;
                    status = false;
                }
            }
            break;

        case PROP_ENABLE:
            status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_BOOLEAN,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                Load_Control_Descr[object_index].Load_Control_Enable = value.type.Boolean;
            }
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
    }

    return status;
}

#ifdef BAC_TEST
#include <assert.h>
#include <string.h>
#include "ctest.h"

#if 0
static void Load_Control_WriteProperty_Request_Shed_Percent(
    Test * pTest,
    int instance,
    unsigned percent)
{
    bool status = false;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_WRITE_PROPERTY_DATA wp_data;

    wp_data.object_type = OBJECT_LOAD_CONTROL;
    wp_data.object_instance = instance;
    wp_data.array_index = BACNET_ARRAY_ALL;
    wp_data.priority = BACNET_NO_PRIORITY;
    wp_data.object_property = PROP_REQUESTED_SHED_LEVEL;
    wp_data.error_class = ERROR_CLASS_PROPERTY;
    wp_data.error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
    value.context_specific = true;
    value.context_tag = 0;
    value.tag = BACNET_APPLICATION_TAG_UNSIGNED_INT;
    value.type.Unsigned_Int = percent;
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[0], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    status = Load_Control_Write_Property(&wp_data);
    ct_test(pTest, status == true);
}
#endif

static void Load_Control_WriteProperty_Request_Shed_Level(
    Test *pTest, int instance, unsigned level)
{
    bool status = false;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_WRITE_PROPERTY_DATA wp_data;

    wp_data.object_type = OBJECT_LOAD_CONTROL;
    wp_data.object_instance = instance;
    wp_data.array_index = BACNET_ARRAY_ALL;
    wp_data.priority = BACNET_NO_PRIORITY;
    wp_data.object_property = PROP_REQUESTED_SHED_LEVEL;
    value.context_specific = true;
    value.context_tag = 1;
    value.tag = BACNET_APPLICATION_TAG_UNSIGNED_INT;
    value.type.Unsigned_Int = level;
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[0], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    status = Load_Control_Write_Property(&wp_data);
    ct_test(pTest, status == true);
}

#if 0
static void Load_Control_WriteProperty_Request_Shed_Amount(
    Test * pTest,
    int instance,
    float amount)
{
    bool status = false;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_WRITE_PROPERTY_DATA wp_data;

    wp_data.object_type = OBJECT_LOAD_CONTROL;
    wp_data.object_instance = instance;
    wp_data.array_index = BACNET_ARRAY_ALL;
    wp_data.priority = BACNET_NO_PRIORITY;
    wp_data.object_property = PROP_REQUESTED_SHED_LEVEL;
    value.context_specific = true;
    value.context_tag = 2;
    value.tag = BACNET_APPLICATION_TAG_REAL;
    value.type.Real = amount;
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[0], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    status = Load_Control_Write_Property(&wp_data);
    ct_test(pTest, status == true);
}
#endif

static void Load_Control_WriteProperty_Enable(
    Test *pTest, int instance, bool enable)
{
    bool status = false;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_WRITE_PROPERTY_DATA wp_data;

    wp_data.object_type = OBJECT_LOAD_CONTROL;
    wp_data.object_instance = instance;
    wp_data.array_index = BACNET_ARRAY_ALL;
    wp_data.priority = BACNET_NO_PRIORITY;
    /* Set Enable=TRUE */
    wp_data.object_property = PROP_ENABLE;
    value.context_specific = false;
    value.context_tag = 0;
    value.tag = BACNET_APPLICATION_TAG_BOOLEAN;
    value.type.Boolean = enable;
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[0], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    status = Load_Control_Write_Property(&wp_data);
    ct_test(pTest, status == true);
}

static void Load_Control_WriteProperty_Shed_Duration(
    Test *pTest, int instance, unsigned duration)
{
    bool status = false;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_WRITE_PROPERTY_DATA wp_data;

    wp_data.object_type = OBJECT_LOAD_CONTROL;
    wp_data.object_instance = instance;
    wp_data.array_index = BACNET_ARRAY_ALL;
    wp_data.priority = BACNET_NO_PRIORITY;
    wp_data.object_property = PROP_SHED_DURATION;
    value.context_specific = false;
    value.context_tag = 0;
    value.tag = BACNET_APPLICATION_TAG_UNSIGNED_INT;
    value.type.Unsigned_Int = duration;
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[0], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    status = Load_Control_Write_Property(&wp_data);
    ct_test(pTest, status == true);
}

static void Load_Control_WriteProperty_Duty_Window(
    Test *pTest, int instance, unsigned duration)
{
    bool status = false;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_WRITE_PROPERTY_DATA wp_data;

    wp_data.object_type = OBJECT_LOAD_CONTROL;
    wp_data.object_instance = instance;
    wp_data.array_index = BACNET_ARRAY_ALL;
    wp_data.priority = BACNET_NO_PRIORITY;
    wp_data.object_property = PROP_DUTY_WINDOW;
    value.context_specific = false;
    value.context_tag = 0;
    value.tag = BACNET_APPLICATION_TAG_UNSIGNED_INT;
    value.type.Unsigned_Int = duration;
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[0], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    status = Load_Control_Write_Property(&wp_data);
    ct_test(pTest, status == true);
}

static void Load_Control_WriteProperty_Start_Time_Wildcards(
    Test *pTest, int instance)
{
    int len = 0;
    bool status = false;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_WRITE_PROPERTY_DATA wp_data;

    wp_data.object_type = OBJECT_LOAD_CONTROL;
    wp_data.object_instance = instance;
    wp_data.array_index = BACNET_ARRAY_ALL;
    wp_data.priority = BACNET_NO_PRIORITY;
    wp_data.object_property = PROP_START_TIME;
    value.context_specific = false;
    value.context_tag = 0;
    value.tag = BACNET_APPLICATION_TAG_DATE;
    datetime_date_wildcard_set(&value.type.Date);
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[0], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    len = wp_data.application_data_len;
    value.tag = BACNET_APPLICATION_TAG_TIME;
    datetime_time_wildcard_set(&value.type.Time);
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[len], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    wp_data.application_data_len += len;
    status = Load_Control_Write_Property(&wp_data);
    ct_test(pTest, status == true);
}

static void Load_Control_WriteProperty_Start_Time(Test *pTest,
    int instance,
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t seconds,
    uint8_t hundredths)
{
    int len = 0;
    bool status = false;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_WRITE_PROPERTY_DATA wp_data;

    wp_data.object_type = OBJECT_LOAD_CONTROL;
    wp_data.object_instance = instance;
    wp_data.array_index = BACNET_ARRAY_ALL;
    wp_data.priority = BACNET_NO_PRIORITY;
    wp_data.object_property = PROP_START_TIME;
    value.context_specific = false;
    value.context_tag = 0;
    value.tag = BACNET_APPLICATION_TAG_DATE;
    datetime_set_date(&value.type.Date, year, month, day);
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[0], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    len = wp_data.application_data_len;
    value.tag = BACNET_APPLICATION_TAG_TIME;
    datetime_set_time(&value.type.Time, hour, minute, seconds, hundredths);
    wp_data.application_data_len =
        bacapp_encode_data(&wp_data.application_data[len], &value);
    ct_test(pTest, wp_data.application_data_len > 0);
    wp_data.application_data_len += len;
    status = Load_Control_Write_Property(&wp_data);
    ct_test(pTest, status == true);
}

void testLoadControlStateMachine(Test *pTest)
{
    unsigned i = 0, j = 0;
    uint8_t level = 0;

    Load_Control_Init();
    /* validate the triggers for each state change */
    for (j = 0; j < 20; j++) {
        Load_Control_State_Machine(0);
        for (i = 0; i < Load_Control_Descr_Size; i++) {
            ct_test(pTest, Load_Control_State[i] == SHED_INACTIVE);
        }
    }
    /* SHED_REQUEST_PENDING */
    /* CancelShed - Start time has wildcards */
    Load_Control_WriteProperty_Enable(pTest, 0, true);
    Load_Control_WriteProperty_Shed_Duration(pTest, 0, 60);
    Load_Control_WriteProperty_Start_Time_Wildcards(pTest, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_INACTIVE);

    /* CancelShed - Requested_Shed_Level equal to default value */
    Load_Control_Init();
    Load_Control_WriteProperty_Request_Shed_Level(pTest, 0, 0);
    Load_Control_WriteProperty_Start_Time(pTest, 0, 2007, 2, 27, 15, 0, 0, 0);
    Load_Control_WriteProperty_Shed_Duration(pTest, 0, 5);
    datetime_set_values(&Current_Time, 2007, 2, 27, 15, 0, 0, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_INACTIVE);

    /* CancelShed - Non-default values, but Start time is passed */
    Load_Control_Init();
    Load_Control_WriteProperty_Enable(pTest, 0, true);
    Load_Control_WriteProperty_Request_Shed_Level(pTest, 0, 1);
    Load_Control_WriteProperty_Shed_Duration(pTest, 0, 5);
    Load_Control_WriteProperty_Start_Time(pTest, 0, 2007, 2, 27, 15, 0, 0, 0);
    datetime_set_values(&Current_Time, 2007, 2, 28, 15, 0, 0, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_INACTIVE);

    /* ReconfigurePending - new write received while pending */
    Load_Control_Init();
    Load_Control_WriteProperty_Enable(pTest, 0, true);
    Load_Control_WriteProperty_Request_Shed_Level(pTest, 0, 1);
    Load_Control_WriteProperty_Shed_Duration(pTest, 0, 5);
    Load_Control_WriteProperty_Start_Time(pTest, 0, 2007, 2, 27, 15, 0, 0, 0);
    datetime_set_values(&Current_Time, 2007, 2, 27, 5, 0, 0, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_WriteProperty_Request_Shed_Level(pTest, 0, 2);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_WriteProperty_Shed_Duration(pTest, 0, 6);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_WriteProperty_Duty_Window(pTest, 0, 60);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_WriteProperty_Start_Time(pTest, 0, 2007, 2, 27, 15, 0, 0, 1);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);

    /* CannotMeetShed -> FinishedUnsuccessfulShed */
    Load_Control_Init();
    Load_Control_WriteProperty_Enable(pTest, 0, true);
    Load_Control_WriteProperty_Request_Shed_Level(pTest, 0, 1);
    Load_Control_WriteProperty_Shed_Duration(pTest, 0, 120);
    Load_Control_WriteProperty_Start_Time(pTest, 0, 2007, 2, 27, 15, 0, 0, 0);
    datetime_set_values(&Current_Time, 2007, 2, 27, 5, 0, 0, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    /* set to lowest value so we cannot meet the shed level */
    datetime_set_values(&Current_Time, 2007, 2, 27, 16, 0, 0, 0);
    Analog_Output_Present_Value_Set(0, 0, 16);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_NON_COMPLIANT);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_NON_COMPLIANT);
    /* FinishedUnsuccessfulShed */
    datetime_set_values(&Current_Time, 2007, 2, 27, 23, 0, 0, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_INACTIVE);

    /* CannotMeetShed -> UnsuccessfulShedReconfigured */
    Load_Control_Init();
    Load_Control_WriteProperty_Enable(pTest, 0, true);
    Load_Control_WriteProperty_Request_Shed_Level(pTest, 0, 1);
    Load_Control_WriteProperty_Shed_Duration(pTest, 0, 120);
    Load_Control_WriteProperty_Start_Time(pTest, 0, 2007, 2, 27, 15, 0, 0, 0);
    datetime_set_values(&Current_Time, 2007, 2, 27, 5, 0, 0, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    /* set to lowest value so we cannot meet the shed level */
    datetime_set_values(&Current_Time, 2007, 2, 27, 16, 0, 0, 0);
    Analog_Output_Present_Value_Set(0, 0, 16);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_NON_COMPLIANT);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_NON_COMPLIANT);
    /* FinishedUnsuccessfulShed */
    Load_Control_WriteProperty_Start_Time(pTest, 0, 2007, 2, 27, 16, 0, 0, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_REQUEST_PENDING);
    datetime_set_values(&Current_Time, 2007, 2, 27, 16, 0, 1, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_NON_COMPLIANT);
    /* CanNowComplyWithShed */
    Analog_Output_Present_Value_Set(0, 100, 16);
    datetime_set_values(&Current_Time, 2007, 2, 27, 16, 0, 2, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_COMPLIANT);
    level = Analog_Output_Present_Value(0);
    ct_test(pTest, level == 90);
    /* FinishedSuccessfulShed */
    datetime_set_values(&Current_Time, 2007, 2, 27, 23, 0, 0, 0);
    Load_Control_State_Machine(0);
    ct_test(pTest, Load_Control_State[0] == SHED_INACTIVE);
    level = Analog_Output_Present_Value(0);
    ct_test(pTest, level == 100);
}

void testLoadControl(Test *pTest)
{
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint16_t decoded_type = 0;
    uint32_t decoded_instance = 0;
    BACNET_READ_PROPERTY_DATA rpdata;

    Analog_Output_Init();
    Load_Control_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_LOAD_CONTROL;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = Load_Control_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_LOAD_CONTROL
int main(void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet Load Control", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testLoadControl);
    assert(rc);
    rc = ct_addTestFunction(pTest, testLoadControlStateMachine);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void)ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif /* TEST_LOAD_CONTROL */
#endif /* BAC_TEST */
