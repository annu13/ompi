/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"


#include "opal/mca/mca.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"


#include "orte/mca/qos/base/base.h"
#include "orte/mca/qos/qos.h"

static int qos_ack_start (void);
static void qos_ack_shutdown (void);
static void* ack_create (opal_list_t *qos_attributes);
static int ack_open (void *qos_channel,
                       opal_buffer_t * buf);
static int ack_send ( void *qos_channel, orte_rml_send_t *msg);
static int ack_recv (void *channel, orte_rml_recv_t *msg);
static void ack_close (void * channel);
static int ack_init_recv (void *channel, opal_list_t *attributes);
static int ack_cmp (void *channel, opal_list_t *attributes);

/**
 * noop module definition
 */
orte_qos_module_t orte_qos_noop_module = {
   ack_create,
   ack_open,
   ack_send,
   ack_recv,
   ack_close,
   ack_init_recv,
   ack_cmp
};

/**
 * component definition
 */
mca_qos_base_component_t mca_qos_ack_component = {
    /* First, the mca_base_component_t struct containing meta
         information about the component itself */

    {
        MCA_QOS_BASE_VERSION_2_0_0,

        "ack", /* MCA component name */
        ORTE_MAJOR_VERSION,  /* MCA component major version */
        ORTE_MINOR_VERSION,  /* MCA component minor version */
        ORTE_RELEASE_VERSION,  /* MCA component release version */
        NULL,
        NULL,
    },
    qos_ack_start,
    qos_ack_shutdown,
    orte_qos_ack,
    {
        ack_create,
        ack_open,
        ack_send,
        ack_recv,
        ack_close,
        ack_init_recv,
        ack_cmp
    }
};

static int qos_ack_start(void) {
    return ORTE_SUCCESS;
}

static void qos_ack_shutdown (void) {
}

static void* ack_create (opal_list_t *qos_attributes) {

    return NULL;
}

static int ack_open (void *qos_channel, opal_buffer_t * buf)  {

    return ORTE_SUCCESS;
}

static int ack_send ( void *qos_channel,  orte_rml_send_t *msg) {

    OPAL_OUTPUT_VERBOSE((1, orte_qos_base_framework.framework_output,
                         "%s ack_send msg = %p to peer = %s\n",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         msg, ORTE_NAME_PRINT(&msg->dst)));
    return ORTE_SUCCESS;
}

static int ack_recv (void *qos_channel, orte_rml_recv_t *msg) {
    OPAL_OUTPUT_VERBOSE((1, orte_qos_base_framework.framework_output,
                         "%s ack_recv msg = %p from peer = %s\n",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         msg, ORTE_NAME_PRINT(&msg->sender)));
    return ORTE_SUCCESS;
}

static void ack_close (void * channel) {

}

static int ack_init_recv (void *channel, opal_list_t *attributes) {
    return ORTE_SUCCESS;
}

static int ack_cmp (void *channel, opal_list_t *attributes) {

}
