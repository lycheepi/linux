/***************************************//*************************************
 * FileName:        observer.h
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
#ifndef MODULES_OBSERVER_H_
#define MODULES_OBSERVER_H_

#include "platform.h"

/** if MAX_OBSERVERS are not defined either at compile time or in platform.h */
#ifndef MAX_OBSERVERS
#define MAX_OBSERVERS       10
#endif ///< MAX_OBSERVERS

typedef void (*EventHandler)(int event, int portId,
                             void *usr_ctx, void *app_ctx);

/**
 * @brief register an observer.
 * @param[in] event to subscribe
 * @param[in] handler to be called
 * @param[in] context data sent to the handler
 */
FSC_BOOL register_observer(int event, EventHandler handler, void *context);

/**
 * @brief removes the observer. Observer stops getting notified
 * @param[in] handler handler to remove
 */
void remove_observer(EventHandler handler);

/**
 * @brief notifies all observer that are listening to the event.
 * @param[in] event that occured
 */
void notify_observers(int event, int portId, void *app_ctx);


#endif /* MODULES_OBSERVER_H_ */
