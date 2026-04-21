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
#ifndef LSP_H
#define LSP_H

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

    typedef struct life_safety_point_descr
    {
        BACNET_LIFE_SAFETY_MODE Mode;
        BACNET_LIFE_SAFETY_STATE State;
        BACNET_SILENCED_STATE Silenced_State;
        BACNET_LIFE_SAFETY_OPERATION Operation;
        bool Out_Of_Service;
        char *Object_Name;
        char *Description;
    }LIFE_SAFETY_POINT_DESCR;

    BACNET_STACK_EXPORT
    void Life_Safety_Point_Property_Lists(
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);
    BACNET_STACK_EXPORT
    bool Life_Safety_Point_Valid_Instance(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    unsigned Life_Safety_Point_Count(
        void);
    BACNET_STACK_EXPORT
    uint32_t Life_Safety_Point_Index_To_Instance(
        unsigned index);
    BACNET_STACK_EXPORT
    unsigned Life_Safety_Point_Instance_To_Index(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Life_Safety_Point_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);
    BACNET_STACK_EXPORT
    bool Life_Safety_Point_Name_Set(uint32_t object_instance, char *new_name);
    BACNET_STACK_EXPORT
    bool Life_Safety_Point_Description(uint32_t instance,
        BACNET_CHARACTER_STRING *object_descr);
    BACNET_STACK_EXPORT
    bool Life_Safety_Point_Description_Set(uint32_t instance, char *new_name);

    BACNET_STACK_EXPORT
    void Life_Safety_Point_Resize(size_t new_size);
    BACNET_STACK_EXPORT
    void Life_Safety_Point_Add(size_t count);
    BACNET_STACK_EXPORT
    void Life_Safety_Point_Alloc(size_t new_size);
    BACNET_STACK_EXPORT
    void Life_Safety_Point_Free(void);
    BACNET_STACK_EXPORT
    void Life_Safety_Point_Objects_Init(void);    

    BACNET_STACK_EXPORT
    void Life_Safety_Point_Init(void);
    BACNET_STACK_EXPORT
    void Life_Safety_Point_Cleanup(void);

    BACNET_STACK_EXPORT
    int Life_Safety_Point_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata);

    BACNET_STACK_EXPORT
    bool Life_Safety_Point_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data);

#ifdef BAC_TEST
#include "ctest.h"
    BACNET_STACK_EXPORT
    void testLifeSafetyPoint(
        Test * pTest);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
