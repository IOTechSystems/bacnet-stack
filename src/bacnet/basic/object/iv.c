/**
 * @file
 * @author Steve Karg
 * @date 2014
 * @brief Integer Value objects, customize for your use
 *
 * @section DESCRIPTION
 *
 * The Integer Value object is an object with a present-value that
 * uses an INTEGER data type.
 *
 * @section LICENSE
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
 */

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
#include "bacnet/bactext.h"
#include "bacnet/config.h" /* the custom stuff */
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/services.h"
/* me! */
#include "bacnet/basic/object/iv.h"


static INTEGER_DESCR *I_Descr = NULL;
static size_t I_Descr_Size = 0;
static pthread_mutex_t I_Descr_Mutex = PTHREAD_MUTEX_INITIALIZER;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Integer_Value_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE, PROP_STATUS_FLAGS,
    PROP_UNITS, -1 };

static const int Integer_Value_Properties_Optional[] = { PROP_OUT_OF_SERVICE, PROP_DESCRIPTION,
    -1 };

static const int Integer_Value_Properties_Proprietary[] = { -1 };

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
void Integer_Value_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Integer_Value_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Integer_Value_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Integer_Value_Properties_Proprietary;
    }

    return;
}

void Integer_Value_Set_Properties(
    uint32_t object_instance, 
    const char* object_name,
    int32_t value,
    bool out_of_service,
    uint16_t units
)
{
    unsigned int index = Integer_Value_Instance_To_Index(object_instance);
    if (index >= I_Descr_Size)
    {
        return;
    }   

    Integer_Value_Name_Set(object_instance, object_name);
    Integer_Value_Present_Value_Set(object_instance, value, false);
    Integer_Value_Out_Of_Service_Set(object_instance, out_of_service);
    Integer_Value_Units_Set(object_instance, units);
}

/**
 * Determines if a given Analog Value instance is valid
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  true if the instance is valid, and false if not
 */
bool Integer_Value_Valid_Instance(uint32_t object_instance)
{
    unsigned int index;

    index = Integer_Value_Instance_To_Index(object_instance);
    if (index < I_Descr_Size) {
        return true;
    }

    return false;
}

/**
 * Determines the number of Analog Value objects
 *
 * @return  Number of Analog Value objects
 */
unsigned Integer_Value_Count(void)
{
    return I_Descr_Size;
}

/**
 * Determines the object instance-number for a given 0..N index
 * of Analog Value objects where N is Integer_Value_Count().
 *
 * @param  index - 0..Integer_Value_Size value
 *
 * @return  object instance-number for the given index
 */
uint32_t Integer_Value_Index_To_Instance(unsigned index)
{
    return index;
}

/**
 * For a given object instance-number, determines a 0..N index
 * of Analog Value objects where N is Integer_Value_Count().
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  index for the given instance-number, or MAX_INTEGER_VALUES
 * if not valid.
 */
unsigned Integer_Value_Instance_To_Index(uint32_t object_instance)
{
    unsigned index = I_Descr_Size;

     if (object_instance < I_Descr_Size) {
        index = object_instance;
    }

    return index;
}

/**
 * For a given object instance-number, determines the present-value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  present-value of the object
 */
int32_t Integer_Value_Present_Value(uint32_t object_instance)
{
    int32_t value = 0;
    unsigned int index;

    index = Integer_Value_Instance_To_Index(object_instance);
    if (index < I_Descr_Size) {
        pthread_mutex_lock(&I_Descr_Mutex);
        value = I_Descr[index].Present_Value;
        pthread_mutex_unlock(&I_Descr_Mutex);
    }

    return value;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - integer value
 *
 * @return  true if values are within range and present-value is set.
 */
bool Integer_Value_Present_Value_Set(
    uint32_t object_instance, int32_t value, uint8_t priority)
{
    bool status = false;
    unsigned int index;

    (void)priority;
    index = Integer_Value_Instance_To_Index(object_instance);
    if (index < I_Descr_Size) {
        pthread_mutex_lock(&I_Descr_Mutex);
        I_Descr[index].Present_Value = value;
        pthread_mutex_unlock(&I_Descr_Mutex);
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, loads the object-name into
 * a characterstring. Note that the object name must be unique
 * within this device.
 *
 * @param  object_instance - object-instance number of the object
 * @param  object_name - holds the object-name retrieved
 *
 * @return  true if object-name was retrieved
 */
bool Integer_Value_Object_Name( uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    static char text_string[32] = ""; /* okay for single thread */
    unsigned int index;
    bool status = false;

    index = Integer_Value_Instance_To_Index(object_instance);
    if (index >= I_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&I_Descr_Mutex);
    if (NULL != I_Descr[index].Name)
    {
        snprintf(text_string, 32, "%s", I_Descr[index].Name);   
    }
    else
    {
        sprintf(text_string, "INTEGER VALUE %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&I_Descr_Mutex);

    status = characterstring_init_ansi(object_name, text_string);

    return status;
}

bool Integer_Value_Name_Set(uint32_t object_instance, const char *new_name)
{
    if (NULL == I_Descr) return false;

    unsigned int index;
    index = Integer_Value_Instance_To_Index(object_instance);
    if (index >= I_Descr_Size)
    {
        return false;
    }

    pthread_mutex_lock(&I_Descr_Mutex);
    free(I_Descr[index].Name);
    I_Descr[index].Name = calloc(strlen(new_name) + 1, sizeof(char));
    if (NULL != I_Descr[index].Name)
    {
        strcpy(I_Descr[index].Name, new_name);
    }
    pthread_mutex_unlock(&I_Descr_Mutex);

    return true;
}

bool Integer_Value_Description(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_descr)
{
    static char text_string[32] = "";
    unsigned int index;
    bool status = false;

    index = Integer_Value_Instance_To_Index(object_instance);
    if (index >= I_Descr_Size) {
        return status;
    }

    pthread_mutex_lock(&I_Descr_Mutex);
    if (NULL != I_Descr[index].Description) {
        snprintf(text_string, 32, "%s", I_Descr[index].Description);
    } else {
        sprintf(text_string, "INTEGER VALUE %lu", (unsigned long)index);
    }
    pthread_mutex_unlock(&I_Descr_Mutex);

    status = characterstring_init_ansi(object_descr, text_string);

    return status;
}

bool Integer_Value_Description_Set(uint32_t object_instance, char *new_descr)
{
    if (NULL == I_Descr) return false;

    unsigned int index;
    index = Integer_Value_Instance_To_Index(object_instance);
    if (index >= I_Descr_Size)
    {
        return false;
    }

    pthread_mutex_lock(&I_Descr_Mutex);
    free(I_Descr[index].Description);
    I_Descr[index].Description = calloc(strlen(new_descr) + 1, sizeof(char));
    if (NULL != I_Descr[index].Description)
    {
        strcpy(I_Descr[index].Description, new_descr);
    }
    pthread_mutex_unlock(&I_Descr_Mutex);

    return true;
}

/**
 * For a given object instance-number, returns the units property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  units property value
 */
uint16_t Integer_Value_Units(uint32_t instance)
{
    unsigned int index;
    uint16_t units = UNITS_NO_UNITS;

    index = Integer_Value_Instance_To_Index(instance);
    if (index < I_Descr_Size) {
        pthread_mutex_lock(&I_Descr_Mutex);
        units = I_Descr[index].Units;
        pthread_mutex_unlock(&I_Descr_Mutex);
    }

    return units;
}

/**
 * For a given object instance-number, sets the units property value
 *
 * @param object_instance - object-instance number of the object
 * @param units - units property value
 *
 * @return true if the units property value was set
 */
bool Integer_Value_Units_Set(uint32_t instance, uint16_t units)
{
    unsigned int index = 0;
    bool status = false;

    index = Integer_Value_Instance_To_Index(instance);
    if (index < I_Descr_Size) {
        pthread_mutex_lock(&I_Descr_Mutex);
        I_Descr[index].Units = units;
        pthread_mutex_unlock(&I_Descr_Mutex);
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, returns the out-of-service
 * property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  out-of-service property value
 */
bool Integer_Value_Out_Of_Service(uint32_t instance)
{
    unsigned int index = 0;
    bool value = false;

    index = Integer_Value_Instance_To_Index(instance);
    if (index < I_Descr_Size) {
        pthread_mutex_lock(&I_Descr_Mutex);
        value = I_Descr[index].Out_Of_Service;
        pthread_mutex_unlock(&I_Descr_Mutex);
    }

    return value;
}

/**
 * For a given object instance-number, sets the out-of-service property value
 *
 * @param object_instance - object-instance number of the object
 * @param value - boolean out-of-service value
 *
 * @return true if the out-of-service property value was set
 */
void Integer_Value_Out_Of_Service_Set(uint32_t instance, bool value)
{
    unsigned int index = 0;

    index = Integer_Value_Instance_To_Index(instance);
    if (index < I_Descr_Size) {
        pthread_mutex_lock(&I_Descr_Mutex);
        I_Descr[index].Out_Of_Service = value;
        pthread_mutex_unlock(&I_Descr_Mutex);
    }
}

/**
 * ReadProperty handler for this object.  For the given ReadProperty
 * data, the application_data is loaded or the error flags are set.
 *
 * @param  rpdata - BACNET_READ_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 *
 * @return number of APDU bytes in the response, or
 * BACNET_STATUS_ERROR on error.
 */
int Integer_Value_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    uint8_t *apdu = NULL;
    uint32_t units = 0;
    int32_t integer_value = 0.0;
    bool state = false;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    apdu = rpdata->application_data;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_INTEGER_VALUE, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Integer_Value_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_DESCRIPTION:
            Integer_Value_Description(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], OBJECT_INTEGER_VALUE);
            break;
        case PROP_PRESENT_VALUE:
            integer_value =
                Integer_Value_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_signed(&apdu[0], integer_value);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            state = Integer_Value_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_OUT_OF_SERVICE:
            state = Integer_Value_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_UNITS:
            units = Integer_Value_Units(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], units);
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

/**
 * WriteProperty handler for this object.  For the given WriteProperty
 * data, the application_data is loaded or the error flags are set.
 *
 * @param  wp_data - BACNET_WRITE_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 *
 * @return false if an error is loaded, true if no errors
 */
bool Integer_Value_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
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
    if ((wp_data->object_property != PROP_PRIORITY_ARRAY) &&
        (wp_data->object_property != PROP_EVENT_TIME_STAMPS) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        /*  only array properties can have array options */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_SIGNED_INT,
                    &wp_data->error_class, &wp_data->error_code);
            if (status) {
                Integer_Value_Present_Value_Set(wp_data->object_instance,
                    value.type.Signed_Int, wp_data->priority);
            }
            break;
        case PROP_OUT_OF_SERVICE:
            status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_BOOLEAN,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                Integer_Value_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_NAME:
        case PROP_OBJECT_TYPE:
        case PROP_STATUS_FLAGS:
        case PROP_UNITS:
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

void Integer_Value_Add(size_t count)
{
    size_t prev_size = I_Descr_Size;
    size_t new_size = I_Descr_Size + count;
   
    pthread_mutex_lock(&I_Descr_Mutex);
    INTEGER_DESCR *tmp = realloc(I_Descr, sizeof(*I_Descr) * new_size);
    if (NULL == tmp) //unsuccessful resize
    {
        pthread_mutex_unlock(&I_Descr_Mutex);
        return;
    }
    I_Descr_Size = new_size;
    I_Descr = tmp;
    pthread_mutex_unlock(&I_Descr_Mutex);

    //initialize object properties
    Integer_Value_Objects_Init();

    char name_buffer[64];
    for(size_t i = prev_size; i < new_size; i++ )
    {
        snprintf(name_buffer, 64, "integer_value_%zu", i);

        Integer_Value_Set_Properties(
          i,
          name_buffer,
          i,
          false,
          UNITS_POUNDS_MASS_PER_MINUTE
        );
    }
}

void Integer_Value_Free(void)
{
    if (NULL == I_Descr) return;    

    pthread_mutex_lock(&I_Descr_Mutex);

    for(unsigned int i=0; i < I_Descr_Size; i++)
    {
        free(I_Descr[i].Name);
        free(I_Descr[i].Description);
    }

    free(I_Descr);
    I_Descr = NULL;
    I_Descr_Size = 0;

    pthread_mutex_unlock(&I_Descr_Mutex);
}

void Integer_Value_Objects_Init(void)
{
    unsigned index = 0;

    pthread_mutex_lock(&I_Descr_Mutex);
    for (index = 0; index < I_Descr_Size; index++) {
        I_Descr[index].Present_Value = 0;
        I_Descr[index].Out_Of_Service = false;
        I_Descr[index].Units = UNITS_NO_UNITS;
        I_Descr[index].Name = NULL;
        I_Descr[index].Description = NULL;
    }
    pthread_mutex_unlock(&I_Descr_Mutex);
}

/**
 * Initializes the Integer Value object data
 */
void Integer_Value_Init(void)
{

}

void Integer_Value_Cleanup(void)
{
    Integer_Value_Free();
}
