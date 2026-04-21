/**************************************************************************
 *
 * Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
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

/* Binary Output Objects - customize for your use */

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
#include "bacnet/rp.h"
#include "bacnet/basic/object/bv.h"
#include "bacnet/basic/services.h"

/* When all the priorities are level null, the present value returns */
/* the Relinquish Default value */
#define RELINQUISH_DEFAULT BINARY_INACTIVE
/* Here is our Priority Array.*/
static BINARY_VALUE_DESCR *BV_Descr = NULL;
static size_t BV_Descr_Size = 0;
static pthread_mutex_t BV_Descr_Mutex = PTHREAD_MUTEX_INITIALIZER;
/* Writable out-of-service allows others to play with our Present Value */
/* without changing the physical output */

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Binary_Value_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE, PROP_STATUS_FLAGS,
    PROP_EVENT_STATE, PROP_OUT_OF_SERVICE, -1 };

static const int Binary_Value_Properties_Optional[] = { PROP_DESCRIPTION,
    PROP_PRIORITY_ARRAY, PROP_RELINQUISH_DEFAULT, -1 };

static const int Binary_Value_Properties_Proprietary[] = { -1 };

/**
 * Initialize the pointers for the required, the optional and the properitary
 * value properties.
 *
 * @param pRequired - Pointer to the pointer of required values.
 * @param pOptional - Pointer to the pointer of optional values.
 * @param pProprietary - Pointer to the pointer of properitary values.
 */
void Binary_Value_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Binary_Value_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Binary_Value_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Binary_Value_Properties_Proprietary;
    }
    return;
}

void Binary_Value_Set_Properties(
    uint32_t object_instance,
    const char* object_name,
    BACNET_BINARY_PV value,
    bool out_of_service
)
{
    unsigned int index = Binary_Value_Instance_To_Index(object_instance);
    if (index >= BV_Descr_Size)
    {
        return;
    }   

    Binary_Value_Name_Set(object_instance, object_name);
    Binary_Value_Present_Value_Set(object_instance, value, 16);
    for (uint8_t i = 1; i < BACNET_MAX_PRIORITY; i++)
    {
        Binary_Value_Present_Value_Set(object_instance, BINARY_NULL, i);
    }
    Binary_Value_Out_Of_Service_Set(object_instance, out_of_service);
}


bool Binary_Value_Description(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_descr)
{
    static char text_string[32] = "";
    unsigned int index;
    bool status = false;

    index = Binary_Value_Instance_To_Index(object_instance);
    if (index >= BV_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&BV_Descr_Mutex);
    if (NULL != BV_Descr[index].Description) {
        snprintf(text_string, 32, "%s", BV_Descr[index].Description);
    } else {
        sprintf(text_string, "BINARY VALUE %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&BV_Descr_Mutex);

    status = characterstring_init_ansi(object_descr, text_string);

    return status;
}

bool Binary_Value_Description_Set(uint32_t object_instance, char *new_descr)
{
    if (NULL == BV_Descr) return false;

    unsigned int index;
    index = Binary_Value_Instance_To_Index(object_instance);
    if (index >= BV_Descr_Size)
    {
        return false;
    }

    pthread_mutex_lock(&BV_Descr_Mutex);
    free(BV_Descr[index].Description);
    BV_Descr[index].Description = calloc(strlen(new_descr) + 1, sizeof(char));
    if (NULL != BV_Descr[index].Description)
    {
        strcpy(BV_Descr[index].Description, new_descr);
    }
    pthread_mutex_unlock(&BV_Descr_Mutex);

    return true;
}

void Binary_Value_Add(size_t count)
{
    size_t prev_size = BV_Descr_Size;
    size_t new_size = BV_Descr_Size + count;
   
    pthread_mutex_lock(&BV_Descr_Mutex);
    BINARY_VALUE_DESCR *tmp = realloc(BV_Descr, sizeof(*BV_Descr) * new_size);
    if (NULL == tmp) //unsuccessful resize
    {
        pthread_mutex_unlock(&BV_Descr_Mutex);
        return;
    }
    BV_Descr_Size = new_size;
    BV_Descr = tmp;
    pthread_mutex_unlock(&BV_Descr_Mutex);

    //initialize object properties
    Binary_Value_Objects_Init();

    char name_buffer[64];
    for(size_t i = prev_size; i < new_size; i++ )
    {
        snprintf(name_buffer, 64, "binary_value_%zu", i);

        Binary_Value_Set_Properties(i, name_buffer, BINARY_ACTIVE, false);
    }
}

void Binary_Value_Free(void)
{
    if (NULL == BV_Descr) return;    

    pthread_mutex_lock(&BV_Descr_Mutex);

    for(unsigned int i=0; i < BV_Descr_Size; i++)
    {
        free(BV_Descr[i].Name);
        free(BV_Descr[i].Description);
    }

    free(BV_Descr);
    BV_Descr = NULL;
    BV_Descr_Size = 0;

    pthread_mutex_unlock(&BV_Descr_Mutex);
}

void Binary_Value_Objects_Init(void)
{
    unsigned i, j;

    /* initialize all the analog output priority arrays to NULL */
    pthread_mutex_lock(&BV_Descr_Mutex);
    for (i = 0; i < BV_Descr_Size; i++) {
        for (j = 0; j < BACNET_MAX_PRIORITY; j++) {
            BV_Descr[i].Level[j] = BINARY_NULL;
        }
        BV_Descr[i].Name = NULL;
        BV_Descr[i].Description = NULL;
    }
    pthread_mutex_unlock(&BV_Descr_Mutex);

}

/**
 * Initialize the binary values.
 */
void Binary_Value_Init(void)
{

}

void Binary_Value_Cleanup(void)
{
    Binary_Value_Free();
}

/**
 * We simply have 0-n object instances. Yours might be
 * more complex, and then you need validate that the
 * given instance exists.
 *
 * @param object_instance Object instance
 *
 * @return true/false
 */
bool Binary_Value_Valid_Instance(uint32_t object_instance)
{
    if (object_instance < BV_Descr_Size) {
        return true;
    }

    return false;
}

/**
 * Return the count of analog values.
 *
 * @return Count of binary values.
 */
unsigned Binary_Value_Count(void)
{
    return BV_Descr_Size;
}

/**
 * We simply have 0-n object instances.  Yours might be
 * more complex, and then you need to return the instance
 * that correlates to the correct index.
 *
 * @param index Index
 *
 * @return Object instance
 */
uint32_t Binary_Value_Index_To_Instance(unsigned index)
{
    return index;
}

/**
 * We simply have 0-n object instances. Yours might be
 * more complex, and then you need to return the index
 * that correlates to the correct instance number
 *
 * @param object_instance Object instance
 *
 * @return Index in the object table.
 */
unsigned Binary_Value_Instance_To_Index(uint32_t object_instance)
{
    unsigned index = BV_Descr_Size;

    if (object_instance < BV_Descr_Size) {
        index = object_instance;
    }

    return index;
}

/**
 * For a given object instance-number, return the present value.
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  Present value
 */
BACNET_BINARY_PV Binary_Value_Present_Value(uint32_t object_instance)
{
    BACNET_BINARY_PV value = RELINQUISH_DEFAULT;
    unsigned index = 0;
    unsigned i = 0;

    index = Binary_Value_Instance_To_Index(object_instance);
    if (index < BV_Descr_Size) {
        pthread_mutex_lock(&BV_Descr_Mutex);
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (BV_Descr[index].Level[i] != BINARY_NULL) {
                value = BV_Descr[index].Level[i];
                break;
            }
        }
        pthread_mutex_unlock(&BV_Descr_Mutex);
    }

    return value;
}

bool Binary_Value_Present_Value_Set(
        uint32_t object_instance,
        BACNET_BINARY_PV binary_value,
        unsigned priority)
{
    unsigned index = 0;
    bool status = false;

    index = Binary_Value_Instance_To_Index(object_instance);
    if (index < BV_Descr_Size) {
        if (priority && (priority <= BACNET_MAX_PRIORITY)) {
            pthread_mutex_lock(&BV_Descr_Mutex);
            BV_Descr[index].Level[priority -1] = binary_value;
            pthread_mutex_unlock(&BV_Descr_Mutex);
            status = true;
        }
    }
    return status;
}

/**
 * For a given object instance-number, return the name.
 *
 * Note: the object name must be unique within this device
 *
 * @param  object_instance - object-instance number of the object
 * @param  object_name - object name/string pointer
 *
 * @return  true/false
 */
bool Binary_Value_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    static char text_string[32] = ""; /* okay for single thread */
    unsigned int index;
    bool status = false;

    index = Binary_Value_Instance_To_Index(object_instance);
    if (index >= BV_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&BV_Descr_Mutex);
    if (NULL != BV_Descr[index].Name)
    {
        snprintf(text_string, 32, "%s", BV_Descr[index].Name);   
    }
    else
    {
        sprintf(text_string, "BINARY VALUE %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&BV_Descr_Mutex);

    status = characterstring_init_ansi(object_name, text_string);

    return status;
}


bool Binary_Value_Name_Set(uint32_t object_instance, const char *new_name)
{
    if (NULL == BV_Descr) return false;

    unsigned int index;
    index =Binary_Value_Instance_To_Index(object_instance);
    if (index >= BV_Descr_Size)
    {
        return false;
    }

    pthread_mutex_lock(&BV_Descr_Mutex);
    free(BV_Descr[index].Name);
    BV_Descr[index].Name = calloc(strlen(new_name) + 1, sizeof(char));
    if (NULL != BV_Descr[index].Name)
    {
        strcpy(BV_Descr[index].Name, new_name);
    }
    pthread_mutex_unlock(&BV_Descr_Mutex);

    return true;
}

/**
 * Return the OOO value, if any.
 *
 * @param instance Object instance.
 *
 * @return true/false
 */
bool Binary_Value_Out_Of_Service(uint32_t instance)
{
    unsigned index = 0;
    bool oos_flag = false;

    index = Binary_Value_Instance_To_Index(instance);
    if (index < BV_Descr_Size) {
        pthread_mutex_lock(&BV_Descr_Mutex);
        oos_flag = BV_Descr[index].Out_Of_Service;
        pthread_mutex_unlock(&BV_Descr_Mutex);

    }

    return oos_flag;
}

/**
 * Set the OOO value, if any.
 *
 * @param instance Object instance.
 * @param oos_flag New OOO value.
 */
void Binary_Value_Out_Of_Service_Set(uint32_t instance, bool oos_flag)
{
    unsigned index = 0;

    index = Binary_Value_Instance_To_Index(instance);
    if (index < BV_Descr_Size) {
        pthread_mutex_lock(&BV_Descr_Mutex);
        BV_Descr[index].Out_Of_Service = oos_flag;
        pthread_mutex_unlock(&BV_Descr_Mutex);
    }
}

/**
 * Return the requested property of the binary value.
 *
 * @param rpdata  Property requested, see for BACNET_READ_PROPERTY_DATA details.
 *
 * @return apdu len, or BACNET_STATUS_ERROR on error
 */
int Binary_Value_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int len = 0;
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    BACNET_BINARY_PV present_value = BINARY_INACTIVE;
    unsigned object_index = 0;
    unsigned i = 0;
    bool state = false;
    uint8_t *apdu = NULL;

    /* Valid data? */
    if (rpdata == NULL) {
        return 0;
    }
    if ((rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    /* Valid object index? */
    object_index = Binary_Value_Instance_To_Index(rpdata->object_instance);
    if (object_index >= BV_Descr_Size) {
        rpdata->error_class = ERROR_CLASS_OBJECT;
        rpdata->error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return BACNET_STATUS_ERROR;
    }

    apdu = rpdata->application_data;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_BINARY_VALUE, rpdata->object_instance);
            break;
            /* note: Name and Description don't have to be the same.
               You could make Description writable and different */
        case PROP_OBJECT_NAME:
            if (Binary_Value_Object_Name(
                    rpdata->object_instance, &char_string)) {
                apdu_len =
                    encode_application_character_string(&apdu[0], &char_string);
            }
            break;
        case PROP_DESCRIPTION:
            if (Binary_Value_Object_Description(
                    rpdata->object_instance, &char_string)) {
                apdu_len =
                    encode_application_character_string(&apdu[0], &char_string);
            }
            break;
        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], OBJECT_BINARY_VALUE);
            break;
        case PROP_PRESENT_VALUE:
            present_value = Binary_Value_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;
        case PROP_STATUS_FLAGS:
            /* note: see the details in the standard on how to use these */
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            state = Binary_Value_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_STATE:
            /* note: see the details in the standard on how to use this */
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
            break;
        case PROP_OUT_OF_SERVICE:
            state = Binary_Value_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
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
                pthread_mutex_lock(&BV_Descr_Mutex);

                for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
                    /* FIXME: check if we have room before adding it to APDU */
                    if (BV_Descr[object_index].Level[i] == BINARY_NULL) {
                        len = encode_application_null(&apdu[apdu_len]);
                    } else {
                        present_value = BV_Descr[object_index].Level[i];
                        len = encode_application_enumerated(
                            &apdu[apdu_len], present_value);
                    }
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
                pthread_mutex_unlock(&BV_Descr_Mutex);
            } else {
                if (rpdata->array_index <= BACNET_MAX_PRIORITY) {
                    pthread_mutex_lock(&BV_Descr_Mutex);

                    if (BV_Descr[object_index].Level[rpdata->array_index] ==
                        BINARY_NULL) {
                        apdu_len = encode_application_null(&apdu[apdu_len]);
                    } else {
                        present_value = BV_Descr[object_index].Level[rpdata->array_index];
                        apdu_len = encode_application_enumerated(
                            &apdu[apdu_len], present_value);
                    }

                    pthread_mutex_unlock(&BV_Descr_Mutex);

                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;
        case PROP_RELINQUISH_DEFAULT:
            present_value = RELINQUISH_DEFAULT;
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }

    /* Only array properties can have array options. */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/**
 * Set the requested property of the binary value.
 *
 * @param wp_data  Property requested, see for BACNET_WRITE_PROPERTY_DATA
 * details.
 *
 * @return true if successful
 */
bool Binary_Value_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    unsigned int object_index = 0;
    unsigned int priority = 0;
    BACNET_BINARY_PV level = BINARY_NULL;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;

    /* Valid data? */
    if (wp_data == NULL) {
        return false;
    }
    if (wp_data->application_data_len == 0) {
        return false;
    }

    /* Decode the some of the request. */
    len = bacapp_decode_application_data(
        wp_data->application_data, wp_data->application_data_len, &value);
    /* FIXME: len < application_data_len: more data? */
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    /* Only array properties can have array options. */
    if ((wp_data->object_property != PROP_PRIORITY_ARRAY) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }

    /* Valid object index? */
    object_index = Binary_Value_Instance_To_Index(wp_data->object_instance);
    if (object_index >= BV_Descr_Size) {
        wp_data->error_class = ERROR_CLASS_OBJECT;
        wp_data->error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return false;
    }

    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            if (value.tag == BACNET_APPLICATION_TAG_ENUMERATED) {
                priority = wp_data->priority;
                /* Command priority 6 is reserved for use by Minimum On/Off
                   algorithm and may not be used for other purposes in any
                   object. */
                if (priority && (priority <= BACNET_MAX_PRIORITY) &&
                    (priority != 6 /* reserved */) &&
                    (value.type.Enumerated <= MAX_BINARY_PV)) {
                    level = (BACNET_BINARY_PV)value.type.Enumerated;
                    priority--;

                    pthread_mutex_lock(&BV_Descr_Mutex);
                    BV_Descr[object_index].Level[priority] = level;
                    pthread_mutex_unlock(&BV_Descr_Mutex);

                    /* Note: you could set the physical output here if we
                       are the highest priority.
                       However, if Out of Service is TRUE, then don't set the
                       physical output.  This comment may apply to the
                       main loop (i.e. check out of service before changing
                       output) */
                    status = true;
                } else if (priority == 6) {
                    /* Command priority 6 is reserved for use by Minimum On/Off
                       algorithm and may not be used for other purposes in any
                       object. */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            } else {
                status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_NULL,
                    &wp_data->error_class, &wp_data->error_code);
                if (status) {
                    level = BINARY_NULL;
                    priority = wp_data->priority;
                    if (priority && (priority <= BACNET_MAX_PRIORITY)) {
                        priority--;
                        
                        pthread_mutex_lock(&BV_Descr_Mutex);
                        BV_Descr[object_index].Level[priority] = level;
                        pthread_mutex_unlock(&BV_Descr_Mutex);

                        /* Note: you could set the physical output here to the
                           next highest priority, or to the relinquish default
                           if no priorities are set. However, if Out of Service
                           is TRUE, then don't set the physical output.  This
                           comment may apply to the
                           main loop (i.e. check out of service before changing
                           output) */
                    } else {
                        status = false;
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
                Binary_Value_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_NAME:
        case PROP_DESCRIPTION:
        case PROP_OBJECT_TYPE:
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
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
    pValue = pValue;
    ucExpectedTag = ucExpectedTag;
    pErrorClass = pErrorClass;
    pErrorCode = pErrorCode;

    return false;
}

void testBinary_Value(Test *pTest)
{
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint16_t decoded_type = 0;
    uint32_t decoded_instance = 0;
    BACNET_READ_PROPERTY_DATA rpdata;

    Binary_Value_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_BINARY_VALUE;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = Binary_Value_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_BINARY_VALUE
int main(void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet Binary_Value", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testBinary_Value);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void)ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif /* TEST_BINARY_VALUE */
#endif /* BAC_TEST */
