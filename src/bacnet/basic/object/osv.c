/**************************************************************************
 *
 * Copyright (C) 2015 Nikola Jelic <nikola.jelic@euroicc.com>
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

/* OctetString Value Objects - customize for your use */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/bactext.h"
#include "bacnet/config.h" /* the custom stuff */
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/object/osv.h"

static OCTETSTRING_VALUE_DESCR *OSV_Descr = NULL;
static size_t OSV_Descr_Size = 0;
static pthread_mutex_t OSV_Mutex = PTHREAD_MUTEX_INITIALIZER;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int OctetString_Value_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER, PROP_OBJECT_NAME, PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE, PROP_STATUS_FLAGS, -1
};

static const int OctetString_Value_Properties_Optional[] = { PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE, PROP_DESCRIPTION, -1 };

static const int OctetString_Value_Properties_Proprietary[] = { -1 };

void OctetString_Value_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = OctetString_Value_Properties_Required;
    }
    if (pOptional) {
        *pOptional = OctetString_Value_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = OctetString_Value_Properties_Proprietary;
    }

    return;
}



void OctetString_Value_Resize(size_t new_size)
{
    OctetString_Value_Free();
    OctetString_Value_Alloc(new_size);
    OctetString_Value_Objects_Init();
}

void OctetString_Value_Add(size_t count)
{
    size_t new_size = OSV_Descr_Size + count;

    pthread_mutex_lock(&OSV_Mutex);
    OCTETSTRING_VALUE_DESCR *tmp = realloc(OSV_Descr, sizeof(*OSV_Descr) * new_size);
    if (NULL == tmp) {
        pthread_mutex_unlock(&OSV_Mutex);
        return;
    }
    OSV_Descr = tmp;
    OSV_Descr_Size = new_size;
    pthread_mutex_unlock(&OSV_Mutex);

    OctetString_Value_Objects_Init();
}

void OctetString_Value_Free(void)
{
    if (NULL == OSV_Descr) return;

    pthread_mutex_lock(&OSV_Mutex);
    for (unsigned i = 0; i < OSV_Descr_Size; i++) {
        free(OSV_Descr[i].Object_Name);
        free(OSV_Descr[i].Description);
    }
    free(OSV_Descr);
    OSV_Descr = NULL;
    OSV_Descr_Size = 0;
    pthread_mutex_unlock(&OSV_Mutex);
}

void OctetString_Value_Alloc(size_t size)
{
    OSV_Descr = calloc(size, sizeof (*OSV_Descr));
    if (NULL != OSV_Descr)
    {
        OSV_Descr_Size = size;
    }
}

void OctetString_Value_Objects_Init()
{
    unsigned i;

    for (i = 0; i < OSV_Descr_Size; i++) {
        memset(&OSV_Descr[i], 0x00, sizeof(OCTETSTRING_VALUE_DESCR));
        octetstring_init(&OSV_Descr[i].Present_Value, NULL, 0);
        OSV_Descr[i].Object_Name = NULL;
        OSV_Descr[i].Description = NULL;
    }
}

void OctetString_Value_Cleanup()
{
    OctetString_Value_Free();
}



void OctetString_Value_Init(void)
{
  
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool OctetString_Value_Valid_Instance(uint32_t object_instance)
{
    if (object_instance < OSV_Descr_Size) {
        return true;
    }

    return false;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned OctetString_Value_Count(void)
{
    return OSV_Descr_Size;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t OctetString_Value_Index_To_Instance(unsigned index)
{
    return index;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned OctetString_Value_Instance_To_Index(uint32_t object_instance)
{
    unsigned index = OSV_Descr_Size;

    if (object_instance < OSV_Descr_Size) {
        index = object_instance;
    }

    return index;
}

/**
 * For a given object instance-number, sets the present-value at a given
 * priority 1..16.
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - octetstring value
 * @param  priority - priority 1..16
 *
 * @return  true if values are within range and present-value is set.
 */
bool OctetString_Value_Present_Value_Set(
    uint32_t object_instance, BACNET_OCTET_STRING *value, uint8_t priority)
{
    unsigned index = 0;
    bool status = false;

    index = OctetString_Value_Instance_To_Index(object_instance);
    if (index < OSV_Descr_Size) {
        octetstring_copy(&OSV_Descr[index].Present_Value, value);
        status = true;
    }
    return status;
}

BACNET_OCTET_STRING *OctetString_Value_Present_Value(uint32_t object_instance)
{
    BACNET_OCTET_STRING *value = NULL;
    unsigned index = 0;

    index = OctetString_Value_Instance_To_Index(object_instance);
    if (index < OSV_Descr_Size) {
        value = &OSV_Descr[index].Present_Value;
    }

    return value;
}

/* note: the object name must be unique within this device */
bool OctetString_Value_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    static char text_string[32] = ""; /* okay for single thread */
    unsigned index;

    index = OctetString_Value_Instance_To_Index(object_instance);
    if (index >= OSV_Descr_Size) {
        return false;
    }

    pthread_mutex_lock(&OSV_Mutex);
    if (NULL != OSV_Descr[index].Object_Name) {
        snprintf(text_string, 32, "%s", OSV_Descr[index].Object_Name);
    } else {
        sprintf(text_string, "OCTETSTRING VALUE %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&OSV_Mutex);

    return characterstring_init_ansi(object_name, text_string);
}

bool OctetString_Value_Name_Set(uint32_t object_instance, char *new_name)
{
    if (NULL == OSV_Descr) return false;
    unsigned index = OctetString_Value_Instance_To_Index(object_instance);
    if (index >= OSV_Descr_Size) return false;

    pthread_mutex_lock(&OSV_Mutex);
    free(OSV_Descr[index].Object_Name);
    OSV_Descr[index].Object_Name = calloc(strlen(new_name) + 1, sizeof(char));
    if (OSV_Descr[index].Object_Name) {
        strcpy(OSV_Descr[index].Object_Name, new_name);
    }
    pthread_mutex_unlock(&OSV_Mutex);
    return true;
}

bool OctetString_Value_Description(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_descr)
{
    static char text_string[32] = "";
    unsigned index;
    bool status = false;

    index = OctetString_Value_Instance_To_Index(object_instance);
    if (index >= OSV_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&OSV_Mutex);
    if (NULL != OSV_Descr[index].Description) {
        snprintf(text_string, 32, "%s", OSV_Descr[index].Description);
    } else {
        sprintf(text_string, "OCTET STRING VALUE %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&OSV_Mutex);

    status = characterstring_init_ansi(object_descr, text_string);

    return status;
}

bool OctetString_Value_Description_Set(uint32_t object_instance, char *new_name)
{
    if (NULL == OSV_Descr) return false;
    unsigned index = OctetString_Value_Instance_To_Index(object_instance);
    if (index >= OSV_Descr_Size) return false;

    pthread_mutex_lock(&OSV_Mutex);
    free(OSV_Descr[index].Description);
    OSV_Descr[index].Description = calloc(strlen(new_name) + 1, sizeof(char));
    if (OSV_Descr[index].Description) {
        strcpy(OSV_Descr[index].Description, new_name);
    }
    pthread_mutex_unlock(&OSV_Mutex);
    return true;
}

/* return apdu len, or BACNET_STATUS_ERROR on error */
int OctetString_Value_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    BACNET_OCTET_STRING *real_value = NULL;
    unsigned object_index = 0;
    bool state = false;
    uint8_t *apdu = NULL;
    OCTETSTRING_VALUE_DESCR *CurrentAV;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    apdu = rpdata->application_data;

    object_index = OctetString_Value_Instance_To_Index(rpdata->object_instance);
    if (object_index < OSV_Descr_Size) {
        CurrentAV = &OSV_Descr[object_index];
    } else {
        return BACNET_STATUS_ERROR;
    }

    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_OCTETSTRING_VALUE, rpdata->object_instance);
            break;

        case PROP_OBJECT_NAME:
            OctetString_Value_Object_Name(
                rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_DESCRIPTION:
            OctetString_Value_Description(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(
                &apdu[0], OBJECT_OCTETSTRING_VALUE);
            break;

        case PROP_PRESENT_VALUE:
            real_value =
                OctetString_Value_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_octet_string(&apdu[0], real_value);
            break;

        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE,
                CurrentAV->Out_Of_Service);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_EVENT_STATE:
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
            break;

        case PROP_OUT_OF_SERVICE:
            state = CurrentAV->Out_Of_Service;
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->object_property != PROP_EVENT_TIME_STAMPS) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/* returns true if successful */
bool OctetString_Value_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    unsigned int object_index = 0;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;
    OCTETSTRING_VALUE_DESCR *CurrentAV;

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
    if ((wp_data->object_property != PROP_PRIORITY_ARRAY) &&
        (wp_data->object_property != PROP_EVENT_TIME_STAMPS) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        /*  only array properties can have array options */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
    object_index =
        OctetString_Value_Instance_To_Index(wp_data->object_instance);
    if (object_index < OSV_Descr_Size) {
        CurrentAV = &OSV_Descr[object_index];
    } else {
        return false;
    }

    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            if (value.tag == BACNET_APPLICATION_TAG_OCTET_STRING) {
                /* Command priority 6 is reserved for use by Minimum On/Off
                   algorithm and may not be used for other purposes in any
                   object. */
                if (OctetString_Value_Present_Value_Set(
                        wp_data->object_instance, &value.type.Octet_String,
                        wp_data->priority)) {
                    status = true;
                } else if (wp_data->priority == 6) {
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
                status = false;
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
            }
            break;

        case PROP_OUT_OF_SERVICE:
            status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_BOOLEAN,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                CurrentAV->Out_Of_Service = value.type.Boolean;
            }
            break;

        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_NAME:
        case PROP_OBJECT_TYPE:
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
        case PROP_DESCRIPTION:
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

void OctetString_Value_Intrinsic_Reporting(uint32_t object_instance)
{
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

void testOctetString_Value(Test *pTest)
{
    BACNET_READ_PROPERTY_DATA rpdata;
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint16_t decoded_type = 0;
    uint32_t decoded_instance = 0;

    OctetString_Value_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_OCTETSTRING_VALUE;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = OctetString_Value_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_OCTETSTRING_VALUE
int main(void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet OctetString Value", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testOctetString_Value);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void)ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif /* TEST_OCTETSTRING_VALUE */
#endif /* BAC_TEST */
