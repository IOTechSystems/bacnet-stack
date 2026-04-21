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

/* Life Safety Point Objects - customize for your use */

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
#include "bacnet/rp.h"
#include "bacnet/wp.h"
#include "bacnet/basic/object/lsp.h"
#include "bacnet/basic/services.h"
#include "bacnet/proplist.h"

/* Here are our stored levels.*/
static LIFE_SAFETY_POINT_DESCR *LSP_Descr = NULL;
static size_t LSP_Descr_Size = 0;
static pthread_mutex_t LSP_Descr_Mutex = PTHREAD_MUTEX_INITIALIZER;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Life_Safety_Point_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER, PROP_OBJECT_NAME, PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE, PROP_TRACKING_VALUE, PROP_STATUS_FLAGS,
    PROP_EVENT_STATE, PROP_OUT_OF_SERVICE, PROP_RELIABILITY, PROP_MODE,
    PROP_ACCEPTED_MODES, PROP_SILENCED, PROP_OPERATION_EXPECTED, -1
};

static const int Life_Safety_Point_Properties_Optional[] = { PROP_DESCRIPTION,
    -1 };

static const int Life_Safety_Point_Properties_Proprietary[] = { -1 };

/**
 * Returns the list of required, optional, and proprietary properties.
 * Used by ReadPropertyMultiple service.
 *
 * @param pRequired - pointer to list of int terminated by -1, of
 * BACnet required properties for this object.
 * @param pOptional - pointer to list of int terminated by -1, of
 * BACnet optkional properties for this object.
 * @param pProprietary - pointer to list of int terminated by -1, of
 * BACnet proprietary properties for this object.
 */
void Life_Safety_Point_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Life_Safety_Point_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Life_Safety_Point_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Life_Safety_Point_Properties_Proprietary;
    }

    return;
}


void Life_Safety_Point_Resize(size_t new_size)
{
    Life_Safety_Point_Free();
    Life_Safety_Point_Alloc(new_size);
    Life_Safety_Point_Objects_Init();
}

void Life_Safety_Point_Add(size_t count)
{
    size_t new_size = LSP_Descr_Size + count;

    pthread_mutex_lock(&LSP_Descr_Mutex);
    LIFE_SAFETY_POINT_DESCR *tmp = realloc(LSP_Descr, sizeof(*LSP_Descr) * new_size);
    if (NULL == tmp) {
        pthread_mutex_unlock(&LSP_Descr_Mutex);
        return;
    }
    LSP_Descr_Size = new_size;
    LSP_Descr = tmp;
    pthread_mutex_unlock(&LSP_Descr_Mutex);

    Life_Safety_Point_Objects_Init();
}

void Life_Safety_Point_Free(void)
{
    if (NULL == LSP_Descr) return;

    pthread_mutex_lock(&LSP_Descr_Mutex);
    for (unsigned i = 0; i < LSP_Descr_Size; i++) {
        free(LSP_Descr[i].Object_Name);
        free(LSP_Descr[i].Description);
    }
    free(LSP_Descr);
    LSP_Descr = NULL;
    LSP_Descr_Size = 0;
    pthread_mutex_unlock(&LSP_Descr_Mutex);
}

void Life_Safety_Point_Alloc(size_t size)
{
    LSP_Descr = calloc(size, sizeof (*LSP_Descr));
    if (NULL != LSP_Descr)
    {
        LSP_Descr_Size = size;
    }
}

void Life_Safety_Point_Objects_Init()
{
    unsigned i;

    /* initialize all the life safety point properties */
    for (i = 0; i < LSP_Descr_Size; i++) {
        LSP_Descr[i].Mode = LIFE_SAFETY_MODE_DEFAULT;
        LSP_Descr[i].State = LIFE_SAFETY_STATE_QUIET;
        LSP_Descr[i].Silenced_State = SILENCED_STATE_UNSILENCED;
        LSP_Descr[i].Operation = LIFE_SAFETY_OP_NONE;
        LSP_Descr[i].Object_Name = NULL;
        LSP_Descr[i].Description = NULL;
    }

    return;
}

void Life_Safety_Point_Cleanup()
{
    Life_Safety_Point_Free();
}

/**
 * Initializes the Life_Safety_Point object data
 */
void Life_Safety_Point_Init(void)
{

}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool Life_Safety_Point_Valid_Instance(uint32_t object_instance)
{
    if (object_instance < LSP_Descr_Size) {
        return true;
    }

    return false;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Life_Safety_Point_Count(void)
{
    return LSP_Descr_Size;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Life_Safety_Point_Index_To_Instance(unsigned index)
{
    return index;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Life_Safety_Point_Instance_To_Index(uint32_t object_instance)
{
    unsigned index = LSP_Descr_Size;

    if (object_instance < LSP_Descr_Size) {
        index = object_instance;
    }

    return index;
}

static BACNET_LIFE_SAFETY_STATE Life_Safety_Point_Present_Value(
    uint32_t object_instance)
{
    BACNET_LIFE_SAFETY_STATE present_value = LIFE_SAFETY_STATE_QUIET;
    unsigned index = 0;

    index = Life_Safety_Point_Instance_To_Index(object_instance);
    if (index < LSP_Descr_Size) {
        present_value = LSP_Descr[index].State;
    }

    return present_value;
}

/* note: the object name must be unique within this device */
bool Life_Safety_Point_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    static char text_string[32] = ""; /* okay for single thread */
    unsigned index;
    bool status = false;

    index = Life_Safety_Point_Instance_To_Index(object_instance);
    if (index >= LSP_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&LSP_Descr_Mutex);
    if (NULL != LSP_Descr[index].Object_Name) {
        snprintf(text_string, 32, "%s", LSP_Descr[index].Object_Name);
    } else {
        sprintf(text_string, "LS POINT %u", object_instance);
    }
    pthread_mutex_unlock(&LSP_Descr_Mutex);

    status = characterstring_init_ansi(object_name, text_string);

    return status;
}

bool Life_Safety_Point_Name_Set(uint32_t object_instance, char *new_name)
{
    unsigned index = Life_Safety_Point_Instance_To_Index(object_instance);
    if (index >= LSP_Descr_Size) return false;

    pthread_mutex_lock(&LSP_Descr_Mutex);
    free(LSP_Descr[index].Object_Name);
    LSP_Descr[index].Object_Name = calloc(strlen(new_name) + 1, sizeof(char));
    if (LSP_Descr[index].Object_Name) {
        strcpy(LSP_Descr[index].Object_Name, new_name);
    }
    pthread_mutex_unlock(&LSP_Descr_Mutex);
    return true;
}

bool Life_Safety_Point_Description(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_descr)
{
    static char text_string[32] = "";
    unsigned index;
    bool status = false;

    index = Life_Safety_Point_Instance_To_Index(object_instance);
    if (index >= LSP_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&LSP_Descr_Mutex);
    if (NULL != LSP_Descr[index].Description) {
        snprintf(text_string, 32, "%s", LSP_Descr[index].Description);
    } else {
        sprintf(text_string, "LIFE SAFETY POINT %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&LSP_Descr_Mutex);

    status = characterstring_init_ansi(object_descr, text_string);

    return status;
}

bool Life_Safety_Point_Description_Set(uint32_t object_instance, char *new_descr)
{
    unsigned index = Life_Safety_Point_Instance_To_Index(object_instance);
    if (index >= LSP_Descr_Size) return false;

    pthread_mutex_lock(&LSP_Descr_Mutex);
    free(LSP_Descr[index].Description);
    LSP_Descr[index].Description = calloc(strlen(new_descr) + 1, sizeof(char));
    if (LSP_Descr[index].Description) {
        strcpy(LSP_Descr[index].Description, new_descr);
    }
    pthread_mutex_unlock(&LSP_Descr_Mutex);
    return true;
}

/* return apdu len, or BACNET_STATUS_ERROR on error */
int Life_Safety_Point_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int len = 0;
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    BACNET_LIFE_SAFETY_STATE present_value = LIFE_SAFETY_STATE_QUIET;
    BACNET_LIFE_SAFETY_MODE mode = LIFE_SAFETY_MODE_DEFAULT;
    BACNET_SILENCED_STATE silenced_state = SILENCED_STATE_UNSILENCED;
    BACNET_LIFE_SAFETY_OPERATION operation = LIFE_SAFETY_OP_NONE;
    unsigned object_index = 0;
    bool state = false;
    BACNET_RELIABILITY reliability = RELIABILITY_NO_FAULT_DETECTED;
    uint8_t *apdu = NULL;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_LIFE_SAFETY_POINT, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Life_Safety_Point_Object_Name(
                rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_DESCRIPTION:
            Life_Safety_Point_Description(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(
                &apdu[0], OBJECT_LIFE_SAFETY_POINT);
            break;
        case PROP_PRESENT_VALUE:
            present_value =
                Life_Safety_Point_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;
        case PROP_TRACKING_VALUE:
            /* FIXME: tracking value is a local matter how it is derived */
            present_value =
                Life_Safety_Point_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, false);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_STATE:
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
            break;
        case PROP_OUT_OF_SERVICE:
            object_index =
                Life_Safety_Point_Instance_To_Index(rpdata->object_instance);
            state = LSP_Descr[object_index].Out_Of_Service;
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_RELIABILITY:
            /* see standard for details about this property */
            reliability = RELIABILITY_NO_FAULT_DETECTED;
            apdu_len = encode_application_enumerated(&apdu[0], reliability);
            break;
        case PROP_MODE:
            object_index =
                Life_Safety_Point_Instance_To_Index(rpdata->object_instance);
            mode = LSP_Descr[object_index].Mode;
            apdu_len = encode_application_enumerated(&apdu[0], mode);
            break;
        case PROP_ACCEPTED_MODES:
            for (mode = MIN_LIFE_SAFETY_MODE; mode < MAX_LIFE_SAFETY_MODE;
                 mode++) {
                len = encode_application_enumerated(&apdu[apdu_len], mode);
                apdu_len += len;
            }
            break;
        case PROP_SILENCED:
            object_index =
                Life_Safety_Point_Instance_To_Index(rpdata->object_instance);
            silenced_state = LSP_Descr[object_index].Silenced_State;
            apdu_len = encode_application_enumerated(&apdu[0], silenced_state);
            break;
        case PROP_OPERATION_EXPECTED:
            object_index =
                Life_Safety_Point_Instance_To_Index(rpdata->object_instance);
            operation = LSP_Descr[object_index].Operation;
            apdu_len = encode_application_enumerated(&apdu[0], operation);
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
bool Life_Safety_Point_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    unsigned int object_index = 0;
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
        case PROP_MODE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                    &wp_data->error_class, &wp_data->error_code);
            if (status) {
                if (value.type.Enumerated <= MAX_LIFE_SAFETY_MODE) {
                    object_index = Life_Safety_Point_Instance_To_Index(
                        wp_data->object_instance);
                    LSP_Descr[object_index].Mode =
                        (BACNET_LIFE_SAFETY_MODE)value.type.Enumerated;
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
                object_index = Life_Safety_Point_Instance_To_Index(
                    wp_data->object_instance);
                LSP_Descr[object_index].Out_Of_Service =
                    value.type.Boolean;
            }
            break;

        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_NAME:
        case PROP_DESCRIPTION:
        case PROP_OBJECT_TYPE:
        case PROP_PRESENT_VALUE:
        case PROP_TRACKING_VALUE:
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
        case PROP_RELIABILITY:
        case PROP_ACCEPTED_MODES:
        case PROP_SILENCED:
        case PROP_OPERATION_EXPECTED:
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

void testLifeSafetyPoint(Test *pTest)
{
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint16_t decoded_type = 0;
    uint32_t decoded_instance = 0;
    BACNET_READ_PROPERTY_DATA rpdata;

    Life_Safety_Point_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_LIFE_SAFETY_POINT;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = Life_Safety_Point_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_LIFE_SAFETY_POINT
int main(void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet Life Safety Point", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testLifeSafetyPoint);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void)ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif /* TEST_LIFE_SAFETY_POINT */
#endif /* BAC_TEST */
