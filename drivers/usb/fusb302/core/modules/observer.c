/***************************************//*************************************
 * FileName:        observer.c
 * Company:         ON Semiconductor
 *
 * Software License Agreement:
 *
 * The software supplied herewith by ON Semiconductor (the Company)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN AS IS CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *****************************************************************************/
#include "observer.h"

/**
 * Private structure for manipulation list of observers
 */
typedef struct
{
    int event;
    EventHandler event_handler;
    void *context;
} Observer_t;

/**
 * Private list of registered observers. upto MAX_OBSERVERS
 */
typedef struct
{
    int obs_count;
    Observer_t list[MAX_OBSERVERS];
} ObserversList_t;

static ObserversList_t observers = {0};

FSC_BOOL register_observer(int event, EventHandler handler, void *context)
{
    FSC_BOOL status = FALSE;
    if (observers.obs_count < MAX_OBSERVERS)
    {
        observers.list[observers.obs_count].event = event;
        observers.list[observers.obs_count].event_handler = handler;
        observers.list[observers.obs_count].context = context;
        observers.obs_count++;
        status = TRUE;
    }
    return status;
}

/**
 * It is slightly expensive to remove since all pointers have to be checked
 * and moved in array when an observer is deleted.
 */
void remove_observer(EventHandler handler)
{
    int i, j = 0;
    FSC_BOOL status = FALSE;

    /* Move all the observer pointers in arary and over-write the
     * one being deleted */
    for (i = 0; i < observers.obs_count; i++)
    {
        if (observers.list[i].event_handler == handler)
        {
            /* Object found */
            status = TRUE;
            continue;
        }
        observers.list[j] = observers.list[i];
        j++;
    }

    /* If observer was found and removed decrement the count */
    if (status == TRUE)
    {
        observers.obs_count--;
    }
}

void notify_observers(int event, int portId, void *app_ctx)
{
    int i;
    for (i = 0; i < observers.obs_count; i++)
    {
        if (observers.list[i].event & event)
        {
            observers.list[i].event_handler(event, portId,
                                            observers.list[i].context, app_ctx);
        }
    }
}

