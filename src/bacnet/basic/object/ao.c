/**************************************************************************
 *
 * Copyright (C) 2005 Steve Karg <skarg@users.sourceforge.net>
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

/* Analog Output Objects - customize for your use */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/config.h" /* the custom stuff */
#include "bacnet/wp.h"
#include "bacnet/basic/object/ao.h"
#include "bacnet/basic/services.h"


/* we choose to have a NULL level in our system represented by */
/* a particular value.  When the priorities are not in use, they */
/* will be relinquished (i.e. set to the NULL level). */
#define AO_LEVEL_NULL 255
/* When all the priorities are level null, the present value returns */
/* the Relinquish Default value */
#define AO_RELINQUISH_DEFAULT 0
/* Here is our Priority Array.  They are supposed to be Real, but */
/* we don't have that kind of memory, so we will use a single byte */
/* and load a Real for returning the value when asked. */
static ANALOG_OUTPUT_DESCR *AO_Descr = NULL;
static size_t AO_Descr_Size = 0;
static pthread_mutex_t AO_Descr_Mutex = PTHREAD_MUTEX_INITIALIZER;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE, PROP_STATUS_FLAGS,
    PROP_EVENT_STATE, PROP_OUT_OF_SERVICE, PROP_UNITS, PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT, -1 };

static const int Properties_Optional[] = { -1 };

static const int Properties_Proprietary[] = { -1 };

void Analog_Output_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Properties_Required;
    }
    if (pOptional) {
        *pOptional = Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Properties_Proprietary;
    }

    return;
}

    
void Analog_Output_Set_Properties(
    uint32_t object_instance, 
    const char *object_name,
    float value
)
{
    Analog_Output_Name_Set(object_instance, object_name);
    Analog_Output_Present_Value_Set(object_instance, value, 16);
    for (uint8_t i = 1; i < BACNET_MAX_PRIORITY; i++)
    {
        Analog_Output_Present_Value_Set(object_instance, AO_LEVEL_NULL, i);
    }
}

bool Analog_Output_Description(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_descr)
{
    static char text_string[32] = "";
    unsigned int index;
    bool status = false;

    index = Analog_Output_Instance_To_Index(object_instance);
    if (index >= AO_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&AO_Descr_Mutex);
    if (NULL != AO_Descr[index].Description) {
        snprintf(text_string, 32, "%s", AO_Descr[index].Description);
    } else {
        sprintf(text_string, "ANALOG OUTPUT %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&AO_Descr_Mutex);

    status = characterstring_init_ansi(object_descr, text_string);

    return status;
}

bool Analog_Output_Description_Set(uint32_t object_instance, char *new_descr)
{
    if (NULL == AO_Descr) return false;

    unsigned int index;
    index = Analog_Output_Instance_To_Index(object_instance);
    if (index >= AO_Descr_Size)
    {
        return false;
    }

    pthread_mutex_lock(&AO_Descr_Mutex);
    free(AO_Descr[index].Description);
    AO_Descr[index].Description = calloc(strlen(new_descr) + 1, sizeof(char));
    if (NULL != AO_Descr[index].Description)
    {
        strcpy(AO_Descr[index].Description, new_descr);
    }
    pthread_mutex_unlock(&AO_Descr_Mutex);

    return true;
}

void Analog_Output_Add(size_t count)
{
    size_t prev_size = AO_Descr_Size;
    size_t new_size = AO_Descr_Size + count;
   
    pthread_mutex_lock(&AO_Descr_Mutex);
    ANALOG_OUTPUT_DESCR *tmp = realloc(AO_Descr, sizeof(*AO_Descr) * new_size);
    if (NULL == tmp) //unsuccessful resize
    {
        pthread_mutex_unlock(&AO_Descr_Mutex);
        return;
    }
    AO_Descr_Size = new_size;
    AO_Descr = tmp;
    pthread_mutex_unlock(&AO_Descr_Mutex);

    //initialize object properties
    Analog_Output_Objects_Init();
    char name_buffer[64];
    for(size_t i = prev_size; i < new_size; i++ )
    {
        snprintf(name_buffer, 64, "analog_output_%zu", i);

        Analog_Output_Set_Properties(i, name_buffer, (float) i);
    }
}

void Analog_Output_Free(void)
{
    if (NULL == AO_Descr) return;  
    pthread_mutex_lock(&AO_Descr_Mutex);

    for(unsigned int i=0; i < AO_Descr_Size; i++)
    {
        free(AO_Descr[i].Name);
        free(AO_Descr[i].Description);
    }

    free(AO_Descr);
    AO_Descr = NULL;
    AO_Descr_Size = 0;
    
    pthread_mutex_unlock(&AO_Descr_Mutex);
}

void Analog_Output_Objects_Init(void)
{
    unsigned i, j;

    /* initialize all the analog output priority arrays to NULL */
    pthread_mutex_lock(&AO_Descr_Mutex);
    for (i = 0; i < AO_Descr_Size; i++) {
        for (j = 0; j < BACNET_MAX_PRIORITY; j++) {
            AO_Descr[i].Level[j] = AO_LEVEL_NULL;
        }
        AO_Descr[i].Out_Of_Service = false;
        AO_Descr[i].Name = NULL;
        AO_Descr[i].Description = NULL;
    }
    pthread_mutex_unlock(&AO_Descr_Mutex);
    return;
}

void Analog_Output_Init(void)
{

}

void Analog_Output_Cleanup(void)
{
    Analog_Output_Free();
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool Analog_Output_Valid_Instance(uint32_t object_instance)
{
    if (object_instance < AO_Descr_Size) {
        return true;
    }

    return false;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Analog_Output_Count(void)
{
    return AO_Descr_Size;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Analog_Output_Index_To_Instance(unsigned index)
{
    return index;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Analog_Output_Instance_To_Index(uint32_t object_instance)
{
    unsigned index = AO_Descr_Size;

    if (object_instance < AO_Descr_Size) {
        index = object_instance;
    }

    return index;
}

float Analog_Output_Present_Value(uint32_t object_instance)
{
    float value = AO_RELINQUISH_DEFAULT;
    unsigned index = 0;
    unsigned i = 0;

    index = Analog_Output_Instance_To_Index(object_instance);
    if (index < AO_Descr_Size) {
        pthread_mutex_lock(&AO_Descr_Mutex);
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (AO_Descr[index].Level[i] != AO_LEVEL_NULL) {
                value = AO_Descr[index].Level[i];
                break;
            }
        }
        pthread_mutex_unlock(&AO_Descr_Mutex);
    }

    return value;
}

unsigned Analog_Output_Present_Value_Priority(uint32_t object_instance)
{
    unsigned index = 0; /* instance to index conversion */
    unsigned i = 0; /* loop counter */
    unsigned priority = 0; /* return value */

    index = Analog_Output_Instance_To_Index(object_instance);
    if (index < AO_Descr_Size) {
        pthread_mutex_lock(&AO_Descr_Mutex);
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (AO_Descr[index].Level[i] != AO_LEVEL_NULL) {
                priority = i + 1;
                break;
            }
        }
        pthread_mutex_unlock(&AO_Descr_Mutex);
    }

    return priority;
}

bool Analog_Output_Present_Value_Set(
    uint32_t object_instance, float value, unsigned priority)
{
    unsigned index = 0;
    bool status = false;

    index = Analog_Output_Instance_To_Index(object_instance);
    if (index < AO_Descr_Size) {
        if (priority && (priority <= BACNET_MAX_PRIORITY)) {
            pthread_mutex_lock(&AO_Descr_Mutex);
            AO_Descr[index].Level[priority - 1] = value;
            pthread_mutex_unlock(&AO_Descr_Mutex);
            /* Note: you could set the physical output here to the next
               highest priority, or to the relinquish default if no
               priorities are set.
               However, if Out of Service is TRUE, then don't set the
               physical output.  This comment may apply to the
               main loop (i.e. check out of service before changing output) */
            status = true;
        }
    }

    return status;
}

bool Analog_Output_Present_Value_Relinquish(
    uint32_t object_instance, unsigned priority)
{
    unsigned index = 0;
    bool status = false;

    index = Analog_Output_Instance_To_Index(object_instance);
    if (index < AO_Descr_Size) {
        if (priority && (priority <= BACNET_MAX_PRIORITY) &&
            (priority != 6 /* reserved */)) {
            pthread_mutex_lock(&AO_Descr_Mutex);
            AO_Descr[index].Level[priority - 1] = AO_LEVEL_NULL;
            pthread_mutex_unlock(&AO_Descr_Mutex);
            /* Note: you could set the physical output here to the next
               highest priority, or to the relinquish default if no
               priorities are set.
               However, if Out of Service is TRUE, then don't set the
               physical output.  This comment may apply to the
               main loop (i.e. check out of service before changing output) */
            status = true;
        }
    }

    return status;
}

/* note: the object name must be unique within this device */
bool Analog_Output_Object_Name( uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    static char text_string[32] = ""; /* okay for single thread */
    bool status = false;

    if (object_instance >= AO_Descr_Size)
    {
        return status;
    }

    pthread_mutex_lock(&AO_Descr_Mutex);
    if (NULL != AO_Descr[object_instance].Name) 
    {
        snprintf(text_string, 32, "%s", AO_Descr[object_instance].Name); 
    }
    else
    {
        sprintf( text_string, "ANALOG OUTPUT %lu", (unsigned long)object_instance);
    }
    pthread_mutex_unlock(&AO_Descr_Mutex);

    status = characterstring_init_ansi(object_name, text_string);

    return status;
}

bool Analog_Output_Name_Set( uint32_t object_instance, const char *new_name)
{
    if (NULL == AO_Descr) return false;

    unsigned int index;
    index = Analog_Output_Instance_To_Index(object_instance);
    if (index >= AO_Descr_Size)
    {
        return false;
    }

    pthread_mutex_lock(&AO_Descr_Mutex);
    free(AO_Descr[index].Name);
    AO_Descr[index].Name = calloc(strlen(new_name) + 1, sizeof(char));
    if (NULL != AO_Descr[index].Name)
    {
        strcpy(AO_Descr[index].Name, new_name);
    }
    pthread_mutex_unlock(&AO_Descr_Mutex);

    return true;
}

bool Analog_Output_Out_Of_Service(uint32_t instance)
{
    unsigned index = 0;
    bool oos_flag = false;

    index = Analog_Output_Instance_To_Index(instance);
    if (index < AO_Descr_Size) {
        pthread_mutex_lock(&AO_Descr_Mutex);
        oos_flag = AO_Descr[index].Out_Of_Service;
        pthread_mutex_unlock(&AO_Descr_Mutex);
    }

    return oos_flag;
}

void Analog_Output_Out_Of_Service_Set(uint32_t instance, bool oos_flag)
{
    unsigned index = 0;

    index = Analog_Output_Instance_To_Index(instance);
    if (index < AO_Descr_Size) {
        pthread_mutex_lock(&AO_Descr_Mutex);
        AO_Descr[index].Out_Of_Service = oos_flag;
        pthread_mutex_unlock(&AO_Descr_Mutex);
    }
}

uint16_t Analog_Output_Units(uint32_t object_instance)
{
    uint16_t units = UNITS_NO_UNITS;
    index = Analog_Output_Instance_To_Index(object_instance);

    if (index < AO_Descr_Size) {
        units = AO_Descr[index].Units;
    }

    return units;
}

bool Analog_Output_Units_Set(uint32_t object_instance, uint16_t value)
{
    unsigned index = 0;
    bool status = false;
    index = Analog_Output_Instance_To_Index(object_instance);
    if (index < AO_Descr_Size) {
        AO_Descr[index].Units = value;
        status = true;
    }
    return status;
}

/* return apdu len, or BACNET_STATUS_ERROR on error */
int Analog_Output_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int len = 0;
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    float real_value = (float)1.414;
    unsigned object_index = 0;
    unsigned i = 0;
    bool state = false;
    uint8_t *apdu = NULL;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_ANALOG_OUTPUT, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Analog_Output_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_DESCRIPTION:
            Analog_Output_Object_Description(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], OBJECT_ANALOG_OUTPUT);
            break;
        case PROP_PRESENT_VALUE:
            real_value = Analog_Output_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            state = Analog_Output_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_STATE:
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
            break;
        case PROP_OUT_OF_SERVICE:
            state = Analog_Output_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_UNITS:
            apdu_len = encode_application_enumerated(
                &apdu[0], Analog_Output_Units(rpdata->object_instance));
            break;
        case PROP_PRIORITY_ARRAY:
            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0) {
                apdu_len =
                    encode_application_unsigned(&apdu[0], BACNET_MAX_PRIORITY);
                /* if no index was specified, then try to encode the entire list
                 */
                /* into one packet. */
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                object_index =
                    Analog_Output_Instance_To_Index(rpdata->object_instance);
                for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
                    /* FIXME: check if we have room before adding it to APDU */
                    pthread_mutex_lock(&AO_Descr_Mutex);
                    if (AO_Descr[object_index].Level[i] == AO_LEVEL_NULL) {
                        len = encode_application_null(&apdu[apdu_len]);
                    } else {
                        real_value = AO_Descr[object_index].Level[i];
                        len = encode_application_real(
                            &apdu[apdu_len], real_value);
                    }
                    pthread_mutex_unlock(&AO_Descr_Mutex);
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
                object_index =
                    Analog_Output_Instance_To_Index(rpdata->object_instance);
                if (rpdata->array_index <= BACNET_MAX_PRIORITY) {
                    pthread_mutex_lock(&AO_Descr_Mutex);
                    if ( AO_Descr[object_index].Level[rpdata->array_index - 1] == AO_LEVEL_NULL) {
                        apdu_len = encode_application_null(&apdu[0]);
                    } else {
                        real_value = AO_Descr[object_index].Level[rpdata->array_index - 1];
                        apdu_len = encode_application_real(&apdu[0], real_value);
                    }
                    pthread_mutex_unlock(&AO_Descr_Mutex);
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;
        case PROP_RELINQUISH_DEFAULT:
            real_value = AO_RELINQUISH_DEFAULT;
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/* returns true if successful */
bool Analog_Output_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;

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
    if ((wp_data->object_property != PROP_PRIORITY_ARRAY) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            if (value.tag == BACNET_APPLICATION_TAG_REAL) {
                /* Command priority 6 is reserved for use by Minimum On/Off
                   algorithm and may not be used for other purposes in any
                   object. */
                status =
                    Analog_Output_Present_Value_Set(wp_data->object_instance,
                        value.type.Real, wp_data->priority);
                if (wp_data->priority == 6) {
                    /* Command priority 6 is reserved for use by Minimum On/Off
                       algorithm and may not be used for other purposes in any
                       object. */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                } else if (!status) {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            } else {
                status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_NULL,
                    &wp_data->error_class, &wp_data->error_code);
                if (status) {
                    status = Analog_Output_Present_Value_Relinquish(
                        wp_data->object_instance, wp_data->priority);
                    if (!status) {
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    }
                }
            }
            break;
        case PROP_OUT_OF_SERVICE:
            status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_BOOLEAN,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                Analog_Output_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_NAME:
        case PROP_OBJECT_TYPE:
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
        case PROP_UNITS:
        case PROP_PRIORITY_ARRAY:
        case PROP_RELINQUISH_DEFAULT:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }

    return status;
}

#ifdef BAC_TEST
#include <assert.h>
#include <string.h>
#include "ctest.h"

bool WPValidateArgType(BACNET_APPLICATION_DATA_VALUE *pValue,
    uint8_t ucExpectedTag,
    BACNET_ERROR_CLASS *pErrorClass,
    BACNET_ERROR_CODE *pErrorCode)
{
    bool bResult;

    /*
     * start out assuming success and only set up error
     * response if validation fails.
     */
    bResult = true;
    if (pValue->tag != ucExpectedTag) {
        bResult = false;
        *pErrorClass = ERROR_CLASS_PROPERTY;
        *pErrorCode = ERROR_CODE_INVALID_DATA_TYPE;
    }

    return (bResult);
}

void testAnalogOutput(Test *pTest)
{
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint32_t decoded_instance = 0;
    uint16_t decoded_type = 0;
    BACNET_READ_PROPERTY_DATA rpdata;

    Analog_Output_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_ANALOG_OUTPUT;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = Analog_Output_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_ANALOG_OUTPUT
int main(void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet Analog Output", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testAnalogOutput);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void)ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif /* TEST_ANALOG_INPUT */
#endif /* BAC_TEST */
