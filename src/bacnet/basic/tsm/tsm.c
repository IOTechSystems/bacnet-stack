/*####COPYRIGHTBEGIN####
 -------------------------------------------
 Copyright (C) 2005 Steve Karg
 Corrections by Ferran Arumi, 2007, Barcelona, Spain

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to:
 The Free Software Foundation, Inc.
 59 Temple Place - Suite 330
 Boston, MA  02111-1307, USA.

 As a special exception, if other files instantiate templates or
 use macros or inline functions from this file, or you compile
 this file and link it with other works to produce a work based
 on this file, this file does not by itself cause the resulting
 work to be covered by the GNU General Public License. However
 the source code for this file must still be made available in
 accordance with section (3) of the GNU General Public License.

 This exception does not invalidate any other reasons why a work
 based on this file might be covered by the GNU General Public
 License.
 -------------------------------------------
####COPYRIGHTEND####*/
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "bacnet/bits.h"
#include "bacnet/apdu.h"
#include "bacnet/bacaddr.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
#include "bacnet/config.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/binding/address.h"

/** @file tsm.c  BACnet Transaction State Machine operations  */
/* FIXME: modify basic service handlers to use TSM rather than this buffer! */
uint8_t Handler_Transmit_Buffer[MAX_PDU] = { 0 };

#if (MAX_TSM_TRANSACTIONS)

/* Really only needed for segmented messages */
/* and a little for sending confirmed messages */
/* If we are only a server and only initiate broadcasts, */
/* then we don't need a TSM layer. */

/* FIXME: not coded for segmentation */

/** Get TSM Device using bacnet address and return tsm device.
 *  NOTE: Requires linking with BACnet device service to use this function
 *
 * @param address  BACnet address of device
 *
 * @return TSM device
 */
extern tsm_device_t *get_tsm_device(BACNET_ADDRESS *address);

/** Find the given Invoke-Id in the list and
 *  return the index.
 *
 * @param tsm_device TSM device list for specific device
 * @param invokeID  Invoke Id
 *
 * @return Index of the id or MAX_TSM_TRANSACTIONS
 *         if not found
 */
static uint8_t tsm_find_invokeID_index(tsm_device_t *tsm_device, uint8_t invokeID)
{
    unsigned i = 0; /* counter */
    uint8_t index = MAX_TSM_TRANSACTIONS; /* return value */

    const BACNET_TSM_DATA *plist = tsm_device->TSM_List;

    for (i = 0; i < MAX_TSM_TRANSACTIONS; i++, plist++) {
        if (plist->InvokeID == invokeID) {
            index = (uint8_t)i;
            break;
        }
    }

    return index;
}

/** Find the first free index in the TSM table.
 *
 * @return Index of the id or MAX_TSM_TRANSACTIONS
 *         if no entry is free.
 */
static uint8_t tsm_find_first_free_index(tsm_device_t *tsm_device)
{
    unsigned i = 0; /* counter */
    uint8_t index = MAX_TSM_TRANSACTIONS; /* return value */

    const BACNET_TSM_DATA *plist = tsm_device->TSM_List;

    for (i = 0; i < MAX_TSM_TRANSACTIONS; i++, plist++) {
        if (plist->InvokeID == 0) {
            index = (uint8_t)i;
            break;
        }
    }

    return index;
}

/** Check if space for transactions is available for specific device.
 *
 * @param address BACnet address of device
 *
 * @return true/false
 */
bool tsm_transaction_available(BACNET_ADDRESS *address)
{
    bool status = false; /* return value */
    unsigned i = 0; /* counter */

    tsm_device_t *tsm_device = get_tsm_device (address);
    if (!tsm_device) goto DONE;
    const BACNET_TSM_DATA *plist = tsm_device->TSM_List;

    for (i = 0; i < MAX_TSM_TRANSACTIONS; i++, plist++) {
        if (plist->InvokeID == 0) {
            /* one is available! */
            status = true;
            break;
        }
    }
DONE:
    return status;
}

/** Gets the next free invokeID,
 * and reserves a spot in the table
 * returns 0 if none are available.
 *
 * @param address  BACnet address of device
 *
 * @return free invoke ID
 */
uint8_t tsm_next_free_invokeID(BACNET_ADDRESS *address)
{
    uint8_t index = 0;
    uint8_t invokeID = 0;
    bool found = false;
    BACNET_TSM_DATA *plist = NULL;

    tsm_device_t *tsm_device = get_tsm_device (address);
    if (!tsm_device) goto DONE;
    /* Is there even space available? */
    if (tsm_transaction_available(address)) {
        while (!found) {
            index = tsm_find_invokeID_index(tsm_device, tsm_device->Current_Invoke_ID);
            if (index == MAX_TSM_TRANSACTIONS) {
                /* Not found, so this invokeID is not used */
                found = true;
                /* set this id into the table */
                index = tsm_find_first_free_index(tsm_device);
                if (index != MAX_TSM_TRANSACTIONS) {
                    plist = &tsm_device->TSM_List[index];
                    plist->InvokeID = invokeID = tsm_device->Current_Invoke_ID;
                    plist->state = TSM_STATE_IDLE;
                    plist->RequestTimer = apdu_timeout();
                    /* update for the next call or check */
                    tsm_device->Current_Invoke_ID++;
                    /* skip zero - we treat that internally as invalid or no
                     * free */
                    if (tsm_device->Current_Invoke_ID == 0) {
                        tsm_device->Current_Invoke_ID = 1;
                    }
                }
            } else {
                /* found! This invokeID is already used */
                /* try next one */
                tsm_device->Current_Invoke_ID++;
                /* skip zero - we treat that internally as invalid or no free */
                if (tsm_device->Current_Invoke_ID == 0) {
                    tsm_device->Current_Invoke_ID = 1;
                }
            }
        }
    }
DONE:
    return invokeID;
}

/** Set for an unsegmented transaction
 *  the state to await confirmation.
 *
 * @param invokeID  Invoke-ID
 * @param dest  Pointer to the BACnet destination address.
 * @param ndpu_data  Pointer to the NPDU structure.
 * @param apdu  Pointer to the received message.
 * @param apdu_len  Bytes valid in the received message.
 */
void tsm_set_confirmed_unsegmented_transaction(uint8_t invokeID,
    BACNET_ADDRESS *dest,
    BACNET_NPDU_DATA *ndpu_data,
    uint8_t *apdu,
    uint16_t apdu_len)
{
    uint8_t index;
    BACNET_TSM_DATA *plist;

    tsm_device_t *tsm_device = get_tsm_device (dest);
    if (!tsm_device) goto DONE;

    if (invokeID && ndpu_data && apdu && (apdu_len > 0)) {
        index = tsm_find_invokeID_index(tsm_device, invokeID);
        if (index < MAX_TSM_TRANSACTIONS) {
            plist = &tsm_device->TSM_List[index];
            /* SendConfirmedUnsegmented */
            plist->state = TSM_STATE_AWAIT_CONFIRMATION;
            plist->RetryCount = 0;
            /* start the timer */
            plist->RequestTimer = apdu_timeout();
            /* copy the data */
            npdu_copy_data(&plist->npdu_data, ndpu_data);
            bacnet_address_copy(&plist->dest, dest);
        }
    }
DONE:
    return;
}

/** Frees the invokeID and sets its state to IDLE
 *
 * @param address  BACnet address of device
 *
 * @param invokeID  Invoke-ID
 */
void tsm_free_invoke_id(BACNET_ADDRESS *address, uint8_t invokeID)
{
    uint8_t index;
    BACNET_TSM_DATA *plist;

    tsm_device_t *tsm_device = get_tsm_device (address);
    if (!tsm_device) return;

    index = tsm_find_invokeID_index(tsm_device, invokeID);
    if (index < MAX_TSM_TRANSACTIONS) {
        plist = &tsm_device->TSM_List[index];
        plist->state = TSM_STATE_IDLE;
        plist->InvokeID = 0;
    }
}

/** Check if the invoke ID has been made free by the Transaction State Machine.
 * @param invokeID [in] The invokeID to be checked, normally of last message
 * sent.
 * @param address  BACnet address of device
 *
 * @return True if it is free (done with), False if still pending in the TSM.
 */
bool tsm_invoke_id_free(BACNET_ADDRESS *address, uint8_t invokeID)
{
    bool status = true;
    uint8_t index;

    tsm_device_t *tsm_device = get_tsm_device (address);
    if (!tsm_device) goto DONE;

    index = tsm_find_invokeID_index(tsm_device, invokeID);
    if (index < MAX_TSM_TRANSACTIONS) {
        status = false;
    }
DONE:
    return status;
}

/** See if we failed get a confirmation for the message associated
 *  with this invoke ID.
 * @param invokeID [in] The invokeID to be checked, normally of last message
 * sent.
 * @param address  BACnet address of device
 *
 * @return True if already failed, False if done or segmented or still waiting
 *         for a confirmation.
 */
bool tsm_invoke_id_failed(BACNET_ADDRESS *address, uint8_t invokeID)
{
    bool status = false;
    uint8_t index;

    tsm_device_t *tsm_device = get_tsm_device (address);
    if (!tsm_device) goto DONE;

    index = tsm_find_invokeID_index(tsm_device, invokeID);
    if (index < MAX_TSM_TRANSACTIONS) {
        /* a valid invoke ID and the state is IDLE is a
           message that failed to confirm */
        if (tsm_device->TSM_List[index].state == TSM_STATE_IDLE) {
            status = true;
        }
    }
DONE:
    return status;
}

#ifdef BAC_TEST
#include <assert.h>
#include <string.h>
#include "ctest.h"

/* flag to send an I-Am */
bool I_Am_Request = true;

/* dummy function stubs */
int datalink_send_pdu(BACNET_ADDRESS *dest,
    BACNET_NPDU_DATA *npdu_data,
    uint8_t *pdu,
    unsigned pdu_len)
{
    (void)dest;
    (void)npdu_data;
    (void)pdu;
    (void)pdu_len;

    return 0;
}

/* dummy function stubs */
void datalink_get_broadcast_address(BACNET_ADDRESS *dest)
{
    (void)dest;
}

void testTSM(Test *pTest)
{
    /* FIXME: add some unit testing... */
    return;
}

#ifdef TEST_TSM
int main(void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet TSM", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testTSM);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void)ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif /* TEST_TSM */
#endif /* BAC_TEST */
#endif /* MAX_TSM_TRANSACTIONS */
