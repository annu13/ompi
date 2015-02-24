/* 
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

/**
 * @file
 *
 * QoS Framework maintenence interface
 *
 *
 * 
 */

#ifndef MCA_QOS_BASE_H
#define MCA_QOS_BASE_H

#include "orte_config.h"
#include "orte/mca/qos/qos.h"
#include "orte/mca/rml/base/base.h"
#include "opal/class/opal_list.h"


/*
 * MCA Framework
 */
ORTE_DECLSPEC extern mca_base_framework_t orte_qos_base_framework;
/* select a component */
ORTE_DECLSPEC int orte_qos_base_select(void);

/* a global struct containing framework-level values */
typedef struct {
    opal_list_t open_channels;
    opal_pointer_array_t actives;
#if OPAL_ENABLE_TIMING
    bool timing;
#endif
} orte_qos_base_t;
ORTE_DECLSPEC extern orte_qos_base_t orte_qos_base;

#define ORTE_QOS_MAX_WINDOW_SIZE 1000

typedef struct orte_qos_base_channel {
    orte_process_name_t *peer;
    opal_list_t attributes;
} orte_qos_base_channel_t;
OBJ_CLASS_DECLARATION(orte_qos_base_channel_t);

/* common implementations */
ORTE_DECLSPEC void* orte_qos_get_module ( opal_list_t *qos_attributes);
int orte_qos_base_pack_attributes (opal_buffer_t * buffer, opal_list_t * qos_attributes);



/* structure to send open channel messages - used internally 
typedef struct {
   opal_list_item_t super;
   orte_qos_channel_t *channel;
   orte_rml_open_channel_t *msg;
   orte_qos_callback_fn_t qos_callback;
   void *cbdata;
} orte_qos_open_channel_t;
OBJ_CLASS_DECLARATION(orte_qos_open_channel_t);

// define an object for transferring send requests to the event lib 
typedef struct {
    opal_object_t super;
    opal_event_t ev;
    orte_qos_open_channel_t post;
} orte_qos_open_channel_request_t;
OBJ_CLASS_DECLARATION(orte_qos_open_channel_request_t);*/



/* common implementations */
/*ORTE_DECLSPEC void orte_qos_base_post_recv(int sd, short args, void *cbdata);
ORTE_DECLSPEC void orte_qos_base_process_msg(int fd, short flags, void *cbdata);
ORTE_DECLSPEC void orte_qos_base_process_error(int fd, short flags, void *cbdata);
ORTE_DECLSPEC void orte_qos_open_channel_recv_callback (int status, orte_process_name_t* peer, 
                                                                struct opal_buffer_t* buffer,
                                                                orte_rml_tag_t tag, void* cbdata);

ORTE_DECLSPEC void orte_qos_open_channel_reply_callback (int status, orte_process_name_t* peer,
                                                         struct opal_buffer_t* buffer,
                                                         orte_rml_tag_t tag, void* cbdata);

ORTE_DECLSPEC void orte_qos_base_open_channel(int fd, short args, void *cbdata); 
#define ORTE_QOS_OPEN_CHANNEL(m)                                                \
    do {                                                                \
    orte_qos_open_channel_request_t *cd;                                        \
    opal_output_verbose(1,                                          \
                        orte_qos_base_framework.framework_output,   \
                        "%s QOS_SEND: %s:%d",                       \
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),         \
                        __FILE__, __LINE__);                        \
    cd = OBJ_NEW(orte_qos_open_channel_request_t);                          \
    cd->post.msg = (m);                                                  \
    opal_event_set(orte_event_base, &cd->ev, -1,                    \
                   OPAL_EV_WRITE,                                   \
                   orte_qos_base_open_channel, cd);                 \
    opal_event_set_priority(&cd->ev, ORTE_MSG_PRI);                 \
    opal_event_active(&cd->ev, OPAL_EV_WRITE, 1);                   \
}while(0); */
END_C_DECLS

#endif /* MCA_QOS_BASE_H */
