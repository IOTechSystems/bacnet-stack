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

/* Binary Input Objects customize for your use */

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
#include "bacnet/rp.h"
#include "bacnet/wp.h"
#include "bacnet/cov.h"
#include "bacnet/config.h" /* the custom stuff */
#include "bacnet/basic/object/bi.h"
#include "bacnet/basic/services.h"

/* stores the current value */
static BINARY_INPUT_DESCR *BI_Descr = NULL;
static size_t BI_Descr_Size = 0;
static pthread_mutex_t BI_Descr_Mutex = PTHREAD_MUTEX_INITIALIZER;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Binary_Input_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE, PROP_STATUS_FLAGS,
    PROP_EVENT_STATE, PROP_OUT_OF_SERVICE, PROP_POLARITY, -1 };

static const int Binary_Input_Properties_Optional[] = { PROP_DESCRIPTION, -1 };

static const int Binary_Input_Properties_Proprietary[] = { -1 };

void Binary_Input_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Binary_Input_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Binary_Input_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Binary_Input_Properties_Proprietary;
    }

    return;
}

void Binary_Input_Set_Properties(
    uint32_t object_instance, 
    const char *object_name, 
    BACNET_BINARY_PV value,
    bool out_of_service,
    BACNET_POLARITY polarity
)
{
    unsigned int index = Binary_Input_Instance_To_Index(object_instance);
    if (index >= BI_Descr_Size)
    {
        return;
    }

    Binary_Input_Name_Set(object_instance, object_name);
    Binary_Input_Present_Value_Set(object_instance, value);
    Binary_Input_Out_Of_Service_Set(object_instance, out_of_service);
    Binary_Input_Polarity_Set(object_instance, polarity);
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool Binary_Input_Valid_Instance(uint32_t object_instance)
{
    if (object_instance < BI_Descr_Size) {
        return true;
    }

    return false;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Binary_Input_Count(void)
{
    return BI_Descr_Size;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Binary_Input_Index_To_Instance(unsigned index)
{
    return index;
}

void Binary_Input_Add(size_t count)
{
    size_t prev_size = BI_Descr_Size;
    size_t new_size = BI_Descr_Size + count;
   
    pthread_mutex_lock(&BI_Descr_Mutex);
    BINARY_INPUT_DESCR *tmp = realloc(BI_Descr, sizeof(*BI_Descr) * new_size);
    if (NULL == tmp) //unsuccessful resize
    {
        pthread_mutex_unlock(&BI_Descr_Mutex);
        return;
    }
    BI_Descr_Size = new_size;
    BI_Descr = tmp;
    pthread_mutex_unlock(&BI_Descr_Mutex);

    //initialize object properties
    char name_buffer[64];
    for(size_t i = prev_size; i < new_size; i++ )
    {
        BI_Descr[i].Name = NULL;
        snprintf(name_buffer, 64, "binary_input_%zu", i);
        Binary_Input_Set_Properties(
            i,
            name_buffer,
            BINARY_ACTIVE,
            false, 
            POLARITY_NORMAL
        );
    }
}

void Binary_Input_Free(void)
{
    if (NULL == BI_Descr) return;    

    pthread_mutex_lock(&BI_Descr_Mutex);
    for(unsigned int i=0; i < BI_Descr_Size; i++)
    {
        free(BI_Descr[i].Name);
    }

    free(BI_Descr);
    BI_Descr = NULL;
    BI_Descr_Size = 0;
    pthread_mutex_unlock(&BI_Descr_Mutex);
}

void Binary_Input_Objects_Init(void)
{
    static bool initialized = false;
    unsigned i;

    if (!initialized) {
        initialized = true;

        /* initialize all the values */
        pthread_mutex_lock(&BI_Descr_Mutex);
        for (i = 0; i < BI_Descr_Size; i++) {
            BI_Descr[i].Present_Value = BINARY_INACTIVE;
            BI_Descr[i].Name = NULL;
        }
        
        pthread_mutex_unlock(&BI_Descr_Mutex);
    }
}

void Binary_Input_Init(void)
{

}

void Binary_Input_Cleanup(void)
{
    Binary_Input_Free();
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Binary_Input_Instance_To_Index(uint32_t object_instance)
{
    unsigned index = BI_Descr_Size;

    if (object_instance < BI_Descr_Size) {
        index = object_instance;
    }

    return index;
}

BACNET_BINARY_PV Binary_Input_Present_Value(uint32_t object_instance)
{
    BACNET_BINARY_PV value = BINARY_INACTIVE;
    unsigned index = 0;

    index = Binary_Input_Instance_To_Index(object_instance);
    if (index < BI_Descr_Size) {
        pthread_mutex_lock(&BI_Descr_Mutex);
        value = BI_Descr[index].Present_Value;
        
        if (BI_Descr[index].Polarity != POLARITY_NORMAL) {
            if (value == BINARY_INACTIVE) {
                value = BINARY_ACTIVE;
            } else {
                value = BINARY_INACTIVE;
            }
        }
        pthread_mutex_unlock(&BI_Descr_Mutex);
    }

    return value;
}

bool Binary_Input_Out_Of_Service(uint32_t object_instance)
{
    bool value = false;
    unsigned index = 0;

    index = Binary_Input_Instance_To_Index(object_instance);
    if (index < BI_Descr_Size) {
        pthread_mutex_lock(&BI_Descr_Mutex);
        value = BI_Descr[index].Out_Of_Service;
        pthread_mutex_unlock(&BI_Descr_Mutex);
    }

    return value;
}

bool Binary_Input_Change_Of_Value(uint32_t object_instance)
{
    bool status = false;
    unsigned index;

    index = Binary_Input_Instance_To_Index(object_instance);
    if (index < BI_Descr_Size) {
        pthread_mutex_lock(&BI_Descr_Mutex);
        status = BI_Descr[index].Change_Of_Value;
        pthread_mutex_unlock(&BI_Descr_Mutex);
    }

    return status;
}

void Binary_Input_Change_Of_Value_Clear(uint32_t object_instance)
{
    unsigned index;

    index = Binary_Input_Instance_To_Index(object_instance);
    if (index < BI_Descr_Size) {
        pthread_mutex_lock(&BI_Descr_Mutex);
        BI_Descr[index].Change_Of_Value = false;
        pthread_mutex_unlock(&BI_Descr_Mutex);
    }

    return;
}

/**
 * For a given object instance-number, loads the value_list with the COV data.
 *
 * @param  object_instance - object-instance number of the object
 * @param  value_list - list of COV data
 *
 * @return  true if the value list is encoded
 */
bool Binary_Input_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list)
{
    bool status = false;

    if (value_list) {
        value_list->propertyIdentifier = PROP_PRESENT_VALUE;
        value_list->propertyArrayIndex = BACNET_ARRAY_ALL;
        value_list->value.context_specific = false;
        value_list->value.tag = BACNET_APPLICATION_TAG_ENUMERATED;
        value_list->value.next = NULL;

        pthread_mutex_lock(&BI_Descr_Mutex);
        value_list->value.type.Enumerated =
            Binary_Input_Present_Value(object_instance);
        pthread_mutex_unlock(&BI_Descr_Mutex);

        value_list->priority = BACNET_NO_PRIORITY;
        value_list = value_list->next;
    }
    if (value_list) {
        value_list->propertyIdentifier = PROP_STATUS_FLAGS;
        value_list->propertyArrayIndex = BACNET_ARRAY_ALL;
        value_list->value.context_specific = false;
        value_list->value.tag = BACNET_APPLICATION_TAG_BIT_STRING;
        value_list->value.next = NULL;
        bitstring_init(&value_list->value.type.Bit_String);
        bitstring_set_bit(
            &value_list->value.type.Bit_String, STATUS_FLAG_IN_ALARM, false);
        bitstring_set_bit(
            &value_list->value.type.Bit_String, STATUS_FLAG_FAULT, false);
        bitstring_set_bit(
            &value_list->value.type.Bit_String, STATUS_FLAG_OVERRIDDEN, false);
        if (Binary_Input_Out_Of_Service(object_instance)) {
            bitstring_set_bit(&value_list->value.type.Bit_String,
                STATUS_FLAG_OUT_OF_SERVICE, true);
        } else {
            bitstring_set_bit(&value_list->value.type.Bit_String,
                STATUS_FLAG_OUT_OF_SERVICE, false);
        }
        value_list->priority = BACNET_NO_PRIORITY;
        value_list->next = NULL;
        status = true;
    }

    return status;
}

bool Binary_Input_Present_Value_Set(
    uint32_t object_instance, BACNET_BINARY_PV value)
{
    unsigned index = 0;
    bool status = false;

    index = Binary_Input_Instance_To_Index(object_instance);
    if (index < BI_Descr_Size) {
        if (BI_Descr[index].Polarity != POLARITY_NORMAL) {
            if (value == BINARY_INACTIVE) {
                value = BINARY_ACTIVE;
            } else {
                value = BINARY_INACTIVE;
            }
        }
        pthread_mutex_lock(&BI_Descr_Mutex);
        if (BI_Descr[index].Present_Value != value) {
            BI_Descr[index].Change_Of_Value = true;
        }
        BI_Descr[index].Present_Value = value;
        pthread_mutex_unlock(&BI_Descr_Mutex);
        status = true;
    }

    return status;
}

void Binary_Input_Out_Of_Service_Set(uint32_t object_instance, bool value)
{
    unsigned index = 0;

    index = Binary_Input_Instance_To_Index(object_instance);
    if (index < BI_Descr_Size) {
        pthread_mutex_lock(&BI_Descr_Mutex);
        if (BI_Descr[index].Out_Of_Service != value) {
            BI_Descr[index].Change_Of_Value = true;
        }
        BI_Descr[index].Out_Of_Service = value;
        pthread_mutex_unlock(&BI_Descr_Mutex);
    }

    return;
}

bool Binary_Input_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    static char text_string[32] = ""; /* okay for single thread */
    unsigned int index;
    bool status = false;

    index = Binary_Input_Instance_To_Index(object_instance);
    if (index >= BI_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&BI_Descr_Mutex);
    if (NULL != BI_Descr[index].Name)
    {
        snprintf(text_string, 32, "%s", BI_Descr[index].Name);   
    }
    else
    {
        sprintf(text_string, "BINARY INPUT %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&BI_Descr_Mutex);

    status = characterstring_init_ansi(object_name, text_string);

    return status;
}

bool Binary_Input_Name_Set(uint32_t object_instance, const char *new_name)
{
    if (NULL == BI_Descr) return false;

    unsigned int index;
    index = Binary_Input_Instance_To_Index(object_instance);
    if (index >= BI_Descr_Size)
    {
        return false;
    }

    pthread_mutex_lock(&BI_Descr_Mutex);
    free(BI_Descr[index].Name);
    BI_Descr[index].Name = calloc(strlen(new_name) + 1, sizeof(char));
    if (NULL != BI_Descr[index].Name)
    {
        strcpy(BI_Descr[index].Name, new_name);
    }
    pthread_mutex_unlock(&BI_Descr_Mutex);

    return true;
}

BACNET_POLARITY Binary_Input_Polarity(uint32_t object_instance)
{
    BACNET_POLARITY polarity = POLARITY_NORMAL;
    unsigned index = 0;

    index = Binary_Input_Instance_To_Index(object_instance);
    if (index < BI_Descr_Size) {
        pthread_mutex_lock(&BI_Descr_Mutex);
        polarity = BI_Descr[index].Polarity;
        pthread_mutex_unlock(&BI_Descr_Mutex);
    }

    return polarity;
}

bool Binary_Input_Polarity_Set(
    uint32_t object_instance, BACNET_POLARITY polarity)
{
    bool status = false;
    unsigned index = 0;

    index = Binary_Input_Instance_To_Index(object_instance);
    if (index < BI_Descr_Size) {
        pthread_mutex_lock(&BI_Descr_Mutex);
        BI_Descr[index].Polarity = polarity;
        pthread_mutex_unlock(&BI_Descr_Mutex);
    }

    return status;
}

/* return apdu length, or BACNET_STATUS_ERROR on error */
/* assumption - object already exists, and has been bounds checked */
int Binary_Input_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    uint8_t *apdu = NULL;
    bool state = false;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_BINARY_INPUT, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
        case PROP_DESCRIPTION:
            /* note: object name must be unique in our device */
            Binary_Input_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], OBJECT_BINARY_INPUT);
            break;
        case PROP_PRESENT_VALUE:
            /* note: you need to look up the actual value */
            apdu_len = encode_application_enumerated(
                &apdu[0], Binary_Input_Present_Value(rpdata->object_instance));
            break;
        case PROP_STATUS_FLAGS:
            /* note: see the details in the standard on how to use these */
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            state = Binary_Input_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_STATE:
            /* note: see the details in the standard on how to use this */
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
            break;
        case PROP_OUT_OF_SERVICE:
            state = Binary_Input_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_POLARITY:
            apdu_len = encode_application_enumerated(
                &apdu[0], Binary_Input_Polarity(rpdata->object_instance));
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/* returns true if successful */
bool Binary_Input_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
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
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                    &wp_data->error_class, &wp_data->error_code);
            if (status) {
                if (value.type.Enumerated <= MAX_BINARY_PV) {
                    Binary_Input_Present_Value_Set(wp_data->object_instance,
                        (BACNET_BINARY_PV)value.type.Enumerated);
                } else {
                    status = false;
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            }
            break;
        case PROP_OUT_OF_SERVICE:
            status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_BOOLEAN,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                Binary_Input_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;
        case PROP_POLARITY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                    &wp_data->error_class, &wp_data->error_code);
            if (status) {
                if (value.type.Enumerated < MAX_POLARITY) {
                    Binary_Input_Polarity_Set(wp_data->object_instance,
                        (BACNET_POLARITY)value.type.Enumerated);
                } else {
                    status = false;
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            }
            break;
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_NAME:
        case PROP_DESCRIPTION:
        case PROP_OBJECT_TYPE:
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
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

void testBinaryInput(Test *pTest)
{
    BACNET_READ_PROPERTY_DATA rpdata;
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint16_t decoded_type = 0;
    uint32_t decoded_instance = 0;

    Binary_Input_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_BINARY_INPUT;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = Binary_Input_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_BINARY_INPUT
int main(void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet Binary Input", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testBinaryInput);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void)ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif /* TEST_BINARY_INPUT */
#endif /* BAC_TEST */
