/* -*- C -*-
 *
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007-2013 Los Alamos National Security, LLC.  All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 *
 */

/*
 * includes
 */
#include "orte_config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "orte/constants.h"
#include "orte/types.h"

#include "opal/dss/dss.h"
#include "opal/util/output.h"
#include "opal/util/timings.h"
#include "opal/class/opal_list.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_wait.h"
#include "orte/util/name_fns.h"

#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/base/base.h"
#include "orte/mca/rml/base/rml_contact.h"
#include "orte/mca/qos/base/base.h"


static void msg_match_recv(orte_rml_posted_recv_t *rcv, bool get_all);
static int orte_qos_base_unpack_attributes (opal_buffer_t *buffer, opal_list_t *qos_attributes);
static orte_rml_channel_t * get_channel ( orte_process_name_t * peer, opal_list_t *qos_attributes);
static int send_open_channel_reply (orte_process_name_t *peer,
                                    orte_rml_channel_t *channel,
                                    bool accept);
void orte_rml_base_post_recv(int sd, short args, void *cbdata)
{
    orte_rml_recv_request_t *req = (orte_rml_recv_request_t*)cbdata;
    orte_rml_posted_recv_t *post, *recv;
    orte_ns_cmp_bitmask_t mask = ORTE_NS_CMP_ALL | ORTE_NS_CMP_WILD;

    opal_output_verbose(5, orte_rml_base_framework.framework_output,
                        "%s posting recv",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    if (NULL == req) {
        /* this can only happen if something is really wrong, but
         * someone managed to get here in a bizarre test */
        opal_output(0, "%s CANNOT POST NULL RML RECV REQUEST",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    post = req->post;

    /* if the request is to cancel a recv, then find the recv
     * and remove it from our list
     */
    if (req->cancel) {
        OPAL_LIST_FOREACH(recv, &orte_rml_base.posted_recvs, orte_rml_posted_recv_t) {
            if (OPAL_EQUAL == orte_util_compare_name_fields(mask, &post->peer, &recv->peer) &&
                post->tag == recv->tag) {
                opal_output_verbose(5, orte_rml_base_framework.framework_output,
                                    "%s canceling recv %d for peer %s",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    post->tag, ORTE_NAME_PRINT(&recv->peer));
                /* got a match - remove it */
                opal_list_remove_item(&orte_rml_base.posted_recvs, &recv->super);
                OBJ_RELEASE(recv);
                break;
            }
        }
        OBJ_RELEASE(req);
        return;
    }

    /* bozo check - cannot have two receives for the same peer/tag combination */
    OPAL_LIST_FOREACH(recv, &orte_rml_base.posted_recvs, orte_rml_posted_recv_t) {
        if (OPAL_EQUAL == orte_util_compare_name_fields(mask, &post->peer, &recv->peer) &&
            post->tag == recv->tag) {
            opal_output(0, "%s TWO RECEIVES WITH SAME PEER %s AND TAG %d - ABORTING",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(&post->peer), post->tag);
            abort();
        }
    }

    opal_output_verbose(5, orte_rml_base_framework.framework_output,
                        "%s posting %s recv on tag %d for peer %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (post->persistent) ? "persistent" : "non-persistent",
                        post->tag, ORTE_NAME_PRINT(&post->peer));
    /* add it to the list of recvs */
    opal_list_append(&orte_rml_base.posted_recvs, &post->super);
    req->post = NULL;
    /* handle any messages that may have already arrived for this recv */
    msg_match_recv(post, post->persistent);

    /* cleanup */
    OBJ_RELEASE(req);
}

static void msg_match_recv(orte_rml_posted_recv_t *rcv, bool get_all)
{
    opal_list_item_t *item, *next;
    orte_rml_recv_t *msg;
    orte_ns_cmp_bitmask_t mask = ORTE_NS_CMP_ALL | ORTE_NS_CMP_WILD;

    /* scan thru the list of unmatched recvd messages and
     * see if any matches this spec - if so, push the first
     * into the recvd msg queue and look no further
     */
    item = opal_list_get_first(&orte_rml_base.unmatched_msgs);
    while (item != opal_list_get_end(&orte_rml_base.unmatched_msgs)) {
        next = opal_list_get_next(item);
        msg = (orte_rml_recv_t*)item;
        opal_output_verbose(5, orte_rml_base_framework.framework_output,
                            "%s checking recv for %s against unmatched msg from %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            ORTE_NAME_PRINT(&rcv->peer),
                            ORTE_NAME_PRINT(&msg->sender));

        /* since names could include wildcards, must use
         * the more generalized comparison function
         */
        if (OPAL_EQUAL == orte_util_compare_name_fields(mask, &msg->sender, &rcv->peer) &&
            msg->tag == rcv->tag) {
            ORTE_RML_ACTIVATE_MESSAGE(msg);
            opal_list_remove_item(&orte_rml_base.unmatched_msgs, item);
            if (!get_all) {
                break;
            }
        }
        item = next;
    }
}

void orte_rml_base_process_msg(int fd, short flags, void *cbdata)
{
    orte_rml_recv_t *msg = (orte_rml_recv_t*)cbdata;
    orte_rml_posted_recv_t *post;
    orte_ns_cmp_bitmask_t mask = ORTE_NS_CMP_ALL | ORTE_NS_CMP_WILD;
    opal_buffer_t buf;

    OPAL_OUTPUT_VERBOSE((5, orte_rml_base_framework.framework_output,
                         "%s message received from %s for tag %d on channel=%d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(&msg->sender),
                         msg->tag,
                         msg->channel_num));

    OPAL_TIMING_EVENT((&tm_rml,"from %s %d bytes",
                       ORTE_NAME_PRINT(&msg->sender), msg->iov.iov_len));

    /* see if we have a waiting recv for this message */
    OPAL_LIST_FOREACH(post, &orte_rml_base.posted_recvs, orte_rml_posted_recv_t) {
        /* since names could include wildcards, must use
         * the more generalized comparison function
         */
        if (OPAL_EQUAL == orte_util_compare_name_fields(mask, &msg->sender, &post->peer) &&
            msg->tag == post->tag) {
            OPAL_OUTPUT_VERBOSE((5, orte_rml_base_framework.framework_output,
                                 "%s message received  bytes from %s for tag %d on channel=%d matched",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_NAME_PRINT(&msg->sender),
                                 msg->tag,
                                 msg->channel_num));

          /*  if ((ORTE_RML_INVALID_CHANNEL_NUM != msg->channel_num) &&
                 (NULL != orte_rml_base_get_channel(msg->channel_num))) {
                // call channel for recv post processing
                orte_rml_base_process_recv_channel (orte_rml_base_get_channel(msg->channel_num), msg);
            }*/
            /* deliver the data to this location */
            if (post->buffer_data) {
                /* deliver it in a buffer */
                OBJ_CONSTRUCT(&buf, opal_buffer_t);
                opal_dss.load(&buf, msg->iov.iov_base, msg->iov.iov_len);
                /* xfer ownership of the malloc'd data to the buffer */
                msg->iov.iov_base = NULL;
                post->cbfunc.buffer(ORTE_SUCCESS, &msg->sender, &buf, msg->tag, post->cbdata);
                /* the user must have unloaded the buffer if they wanted
                 * to retain ownership of it, so release whatever remains
                 */
                OPAL_OUTPUT_VERBOSE((5, orte_rml_base_framework.framework_output,
                                     "%s message received  bytes from %s for tag %d on channel=%d called callback",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(&msg->sender),
                                     msg->tag,
                                     msg->channel_num));
                OBJ_DESTRUCT(&buf);
            } else {
                /* deliver as an iovec */
                post->cbfunc.iov(ORTE_SUCCESS, &msg->sender, &msg->iov, 1, msg->tag, post->cbdata);
                /* the user should have shifted the data to
                 * a local variable and NULL'd the iov_base
                 * if they wanted ownership of the data
                 */
            }
            /* release the message */
            OBJ_RELEASE(msg);
            OPAL_OUTPUT_VERBOSE((5, orte_rml_base_framework.framework_output,
                                 "%s message tag %d on released",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 post->tag));
            /* if the recv is non-persistent, remove it */
            if (!post->persistent) {
                OPAL_OUTPUT_VERBOSE((5, orte_rml_base_framework.framework_output,
                                     "%s non persistent recv %p removing from list",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     post));
                opal_list_remove_item(&orte_rml_base.posted_recvs, &post->super);
                OPAL_OUTPUT_VERBOSE((5, orte_rml_base_framework.framework_output,
                                     "%s non persistent recv %p remove success releasing now",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     post));
                OBJ_RELEASE(post);

            }
            return;
        }
    }

    /* we get here if no matching recv was found - we then hold
     * the message until such a recv is issued
     */
    OPAL_OUTPUT_VERBOSE((5, orte_rml_base_framework.framework_output,
                         "%s message received  bytes from %s for tag %d on channel=%d Not Matched adding to unmatched msgs",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(&msg->sender),
                         msg->tag,
                         msg->channel_num));
    opal_list_append(&orte_rml_base.unmatched_msgs, &msg->super);

}

void orte_rml_base_open_channel(int fd, short flags, void *cbdata)
{
    int32_t *type, type_val;
    orte_rml_send_request_t *req = (orte_rml_send_request_t*)cbdata;
    orte_process_name_t *peer = &(req->post.channel.dst);
    orte_rml_open_channel_t *open_chan;
    orte_rml_channel_t *channel;
    opal_buffer_t *buffer;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel to peer %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(peer)));
    OPAL_TIMING_EVENT((&tm_rml, "to %s", ORTE_NAME_PRINT(peer)));
    channel = OBJ_NEW(orte_rml_channel_t);
    channel->channel_num = opal_pointer_array_add (&orte_rml_base.open_channels, channel);
    channel->peer = *peer;
    open_chan = OBJ_NEW(orte_rml_open_channel_t);
    open_chan->dst = *peer;
    open_chan->qos_attributes = req->post.channel.qos_attributes;
    open_chan->cbfunc = req->post.channel.cbfunc;
    open_chan->cbdata = req->post.channel.cbdata;
    OBJ_RELEASE(req);
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel to peer %s SUCCESS",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(peer)));
     // associate open channel request and the newly created channel object
    open_chan->channel = channel;
    type = &type_val;
    orte_get_attribute( open_chan->qos_attributes, ORTE_QOS_TYPE, (void**)&type, OPAL_UINT8);
    open_chan->channel->qos = (void*) orte_qos_get_module (open_chan->qos_attributes);
    // now associate qos with the channel based on user requested attributes.
    if ( NULL != open_chan->channel->qos)
    {
        open_chan->channel->qos_channel_ptr = orte_qos_create_channel (open_chan->channel->qos, open_chan->qos_attributes);
        // create rml send for open channel request. Call the corresponding QoS module to pack the attributes.
        buffer = OBJ_NEW (opal_buffer_t);
        // call QoS module to pack attributes
        if ( ORTE_SUCCESS == (orte_qos_open_channel(open_chan->channel->qos, open_chan->channel->qos_channel_ptr, buffer)))
        {
            OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                                 "%s rml_open_channel to peer %s SUCCESS sending to peer",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_NAME_PRINT(peer)));
            // now post a recieve for open_channel_response tag
            orte_rml.recv_buffer_nb(peer, ORTE_RML_TAG_OPEN_CHANNEL_RESP,
                                    ORTE_RML_NON_PERSISTENT, orte_rml_open_channel_resp_callback, open_chan);
            //  send request to peer to open channel
            orte_rml.send_buffer_nb( peer, buffer, ORTE_RML_TAG_OPEN_CHANNEL_REQ,
                                  orte_rml_open_channel_send_callback,
                                  open_chan);

        } else {
            open_chan->status = ORTE_ERR_PACK_FAILURE;
            ORTE_RML_OPEN_CHANNEL_COMPLETE(open_chan);
            opal_pointer_array_set_item ( &orte_rml_base.open_channels, open_chan->channel->channel_num, NULL);
            // call QoS module to release the QoS channel object.
            orte_qos_close_channel (open_chan->channel->qos, open_chan->channel->qos_channel_ptr);
            OBJ_RELEASE (buffer);
            OBJ_RELEASE(open_chan->channel);
            OBJ_RELEASE(open_chan);
        }
    }
    else
    {
        // do error completion because a component for the requested QoS does not exist
        open_chan->status = ORTE_ERR_QOS_TYPE_UNSUPPORTED;
        ORTE_RML_OPEN_CHANNEL_COMPLETE(open_chan);
        opal_pointer_array_set_item ( &orte_rml_base.open_channels, open_chan->channel->channel_num, NULL);
        OBJ_RELEASE(open_chan->channel);
        OBJ_RELEASE(open_chan);
    }

}

void orte_rml_open_channel_send_callback ( int status,
        orte_process_name_t* sender,
        opal_buffer_t* buffer,
        orte_rml_tag_t tag,
        void* cbdata)
{
    // this is the send call back for open channel request
    orte_rml_open_channel_t *req = (orte_rml_open_channel_t*) cbdata;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel_send_callback to peer %s status = %d",
                         ORTE_NAME_PRINT(sender),
                         ORTE_NAME_PRINT(&req->dst), status));
    // if the message was not sent we should retry or complete the request appropriately
    if (status!= ORTE_SUCCESS)
    {
        req->status = status;
        ORTE_RML_OPEN_CHANNEL_COMPLETE(req);
        opal_pointer_array_set_item ( &orte_rml_base.open_channels, req->channel->channel_num, NULL);
        // call QoS module to release the QoS channel object.
        orte_qos_close_channel (req->channel->qos, req->channel->qos_channel_ptr);
        OBJ_RELEASE(req->channel);
        OBJ_RELEASE(req);
    }
    else {
        // start a timer for response from peer
    }
    OBJ_RELEASE(buffer);
}

void orte_rml_open_channel_resp_callback (int status,
                                          orte_process_name_t* peer,
                                          struct opal_buffer_t* buffer,
                                          orte_rml_tag_t tag,
                                          void* cbdata)
{
    orte_rml_open_channel_t *req = (orte_rml_open_channel_t*) cbdata;
    orte_rml_channel_t * channel = req->channel;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel_resp_callback to peer %s status = %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(peer), status));
    int32_t rc;
    bool peer_resp = false;
    int32_t count = 1;
    // unpack peer  response from buffer to determine if peer has accepted the open request
    if ((ORTE_SUCCESS == (rc = opal_dss.unpack(buffer, &peer_resp, &count, OPAL_BOOL))) && peer_resp) {

        OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                             "%s rml_open_channel_resp_callback to peer response = %d",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             peer_resp));
        /* response will contain the peer channel number -  the peer does not have the
           option to change the channel attributes */
        // unpack  and get peer channel number.
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &channel->peer_channel, &count, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            req->status = ORTE_ERR_UNPACK_FAILURE;
            opal_pointer_array_set_item ( &orte_rml_base.open_channels, req->channel->channel_num, NULL);
            // call QoS module to release the QoS channel object.
            orte_qos_close_channel (req->channel->qos, req->channel->qos_channel_ptr);
            OBJ_RELEASE(req->channel);
            // TBD : should we send a close channel to the peer??
        }
        else {
            // call qos module to update the channel state.??
            req->status = ORTE_SUCCESS;
            req->channel->state = orte_rml_channel_open;
        }
    }
    else {
        if (rc) {
            ORTE_ERROR_LOG(rc);
            req->status = ORTE_ERR_UNPACK_FAILURE;
        } else {
            req->status = ORTE_ERR_OPEN_CHANNEL_PEER_REJECT;
        }
        opal_pointer_array_set_item ( &orte_rml_base.open_channels, req->channel->channel_num, NULL);
        // call QoS module to release the QoS channel object.
        orte_qos_close_channel (req->channel->qos, req->channel->qos_channel_ptr);
        OBJ_RELEASE(req->channel);
    }
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel_resp_callback to peer %s status = %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(peer), req->status));
    ORTE_RML_OPEN_CHANNEL_COMPLETE(req);
}

static int orte_rml_base_unpack_attributes (opal_buffer_t *buffer,
        opal_list_t *qos_attributes)
{
    orte_attribute_t *kv;
    int32_t count, n, k;
    int32_t rc=ORTE_SUCCESS;
    /* unpack the attributes */
    n=1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &count,
                              &n, ORTE_STD_CNTR))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_unpack_attributes num attributes = %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         count));
    for (k=0; k < count; k++) {
        n=1;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &kv,
                                  &n, ORTE_ATTRIBUTE))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                             "rml_unpack_attributes unpacked attribute key = %d, value = %d ",
                              kv->key,
                              kv->data.uint8));
        kv->local = ORTE_ATTR_GLOBAL;
        opal_list_append(qos_attributes, &kv->super);
    }
    return rc;
}

void orte_rml_open_channel_recv_callback (int status,
        orte_process_name_t* peer,
        struct opal_buffer_t* buffer,
        orte_rml_tag_t tag,
        void* cbdata)
{
    opal_list_t *qos_attributes = OBJ_NEW(opal_list_t);
    orte_rml_channel_t *channel;
    uint8_t *type, type_val = 10;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel_recv_callback from peer %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(peer)));
    /* unpack attributes first */
    if ( ORTE_SUCCESS == orte_rml_base_unpack_attributes( buffer, qos_attributes)) {
        type = &type_val;
        orte_get_attribute( qos_attributes, ORTE_QOS_TYPE, (void**)&type, OPAL_UINT8);
        OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                             "rml_open_channel_recv_callback type =%d",
                             type_val));
        /* scan the list of channels to see if we already have a channel with qos_attributes */
        if (NULL == (channel = get_channel ( peer, qos_attributes))) {
            /* create a new channel for the req */
            channel = OBJ_NEW(orte_rml_channel_t);
            channel->channel_num = opal_pointer_array_add (&orte_rml_base.open_channels, channel);
            OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                                 "rml_open_channel_recv_callback channel num =%d",
                                 channel->channel_num));
            channel->peer = *peer;
            channel->receive = true;
            channel->qos = (void*) orte_qos_get_module (qos_attributes);
            /* now associate qos with the channel based on requested attributes */
            channel->qos_channel_ptr = (void*) orte_qos_create_channel(channel->qos, qos_attributes);
            if (channel->qos_channel_ptr) {
                /* call qos to init recv state */
                 orte_qos_init_recv_channel ( channel->qos, channel->qos_channel_ptr, qos_attributes);
                /* send channel accept reply to sender */
                if(ORTE_SUCCESS == send_open_channel_reply (peer, channel, true))  {
                    /* update channel state */
                    channel->state = orte_rml_channel_open;
                }
                else {
                    /* the receiver shall not attempt to resend  or send a reject message
                     instead we let the sender's request timeout at his end.
                     release the channel etc */
                    opal_pointer_array_set_item ( &orte_rml_base.open_channels, channel->channel_num, NULL);
                    orte_qos_close_channel (channel->qos, channel->qos_channel_ptr);
                    OBJ_RELEASE(channel);
                }
            } else {
                send_open_channel_reply (peer, NULL, false);
                opal_pointer_array_set_item ( &orte_rml_base.open_channels, channel->channel_num, NULL);
                orte_qos_close_channel (channel->qos, channel->qos_channel_ptr);
                OBJ_RELEASE(channel);
            }
        }
        else {
            /*this means that there exists a channel with the same attributes which was
              previously created on user or sender's open channel request
              send channel accept reply to sender */
            if(ORTE_SUCCESS == send_open_channel_reply (peer, channel, true))
                /* exercise caution while updating state of a bidirectional channel*/
                channel->state = orte_rml_channel_open;
            else {
                /* the receiver shall not attempt to resend  or send a reject message
                   instead we let the sender's request timeout at his end.
                   release the channel etc */
                opal_pointer_array_set_item ( &orte_rml_base.open_channels, channel->channel_num, NULL);
                orte_qos_close_channel (channel->qos, channel->qos_channel_ptr);
                OBJ_RELEASE(channel);
            }
        }

    }
    else {
        //reply with error message
        send_open_channel_reply (peer, NULL, false);
    }
}

static int send_open_channel_reply (orte_process_name_t *peer,
                                     orte_rml_channel_t *channel,
                                     bool accept)
{
    opal_buffer_t *buffer;
    int32_t rc;
    buffer = OBJ_NEW (opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &accept , 1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    if (accept) {
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &channel->channel_num , 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }
    /* TBD: should specify reason for reject
      send channel accept to sender */
    orte_rml.send_buffer_nb ( peer, buffer, ORTE_RML_TAG_OPEN_CHANNEL_RESP,
                              orte_rml_open_channel_reply_send_callback,
                              channel);

    return rc;
}

static orte_rml_channel_t * get_channel ( orte_process_name_t * peer, opal_list_t *qos_attributes)
{
    orte_rml_channel_t *channel = NULL;
    int32_t i = 0;
    for (i=0; i < orte_rml_base.open_channels.size; i++) {
        if (NULL != (channel = (orte_rml_channel_t*) opal_pointer_array_get_item (&orte_rml_base.open_channels, i))) {
            /* compare basic properties */
            if ((OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &channel->peer, peer)) &&
                                            ((orte_rml_channel_open == channel->state) ||
                                             (orte_rml_channel_opening == channel->state)))
            {
                /* compare channel attributes */
                if( ORTE_SUCCESS == orte_qos_cmp_channel ( channel->qos, channel->qos_channel_ptr, qos_attributes)) {
                    /* we have an existing channel that we can use */
                    /* make it a receive channel and inform qos to init recv state */
                    channel->receive = true;
                    orte_qos_init_recv_channel ( channel->qos, channel->qos_channel_ptr, qos_attributes);
                    return channel;
                }
                else
                    return NULL;
            }
        }
    }
    return NULL;
}

void orte_rml_open_channel_reply_send_callback ( int status,
        orte_process_name_t* sender,
        opal_buffer_t* buffer,
        orte_rml_tag_t tag,
        void* cbdata)
{
    // this is the send call back for open channel reply
    orte_rml_channel_t *channel = (orte_rml_channel_t*) cbdata;
    // if the message was not sent we should retry or release the channel resources
    if (status!= ORTE_SUCCESS)
    {
        ORTE_ERROR_LOG (status);
        // release channel
        if(NULL != channel) {
            opal_pointer_array_set_item ( &orte_rml_base.open_channels, channel->channel_num, NULL);
            // call QoS module to release the QoS channel object.
            orte_qos_close_channel (channel->qos, channel->qos_channel_ptr);
            OBJ_RELEASE(channel);
        } else {
            // we did not accept the request so nothing to do
        }
    }
    // if success then release the buffer and do open channel request completion after receiving response from peer
    OBJ_RELEASE(buffer);
}

orte_rml_channel_t * orte_rml_base_get_channel (orte_rml_channel_num_t chan_num) {
    orte_rml_channel_t * channel;
    channel = (orte_rml_channel_t*) opal_pointer_array_get_item (&orte_rml_base.open_channels, chan_num);
    if ((NULL != channel) && (orte_rml_channel_open == channel->state))
        return channel;
    else
        return NULL;
}
void orte_rml_base_prep_send_channel (orte_rml_channel_t *channel,
                                      orte_rml_send_t *send)
{
    // add channel number and notify Qos
    send->dst_channel = channel->peer_channel;
    orte_qos_send_channel (channel->qos, channel->qos_channel_ptr, send);
}

void orte_rml_base_process_recv_channel (orte_rml_channel_t *channel,
        orte_rml_recv_t *recv)
{
     // call qos for recv post processing
    orte_qos_recv_channel (channel->qos, channel->qos_channel_ptr, recv);
}
