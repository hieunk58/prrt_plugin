/*
 * GStreamer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#define GST_ENABLE_EXTRA_CHECKS

#include <gst/gst.h>
#include "gstprrtsrc.h"

#include "../../PRRT/prrt/util/dbg.h"
FILE *prrt_debug_log;

#define PRRT_DEFAULT_PORT 5000
#define PRRT_DEFAULT_WINDOW 20000
#define BUFFER_SIZE 4096

GST_DEBUG_CATEGORY_STATIC (gst_prrt_source_debug);
#define GST_CAT_DEFAULT gst_prrt_source_debug

enum {
    /* methods */
    SIGNAL_GET_STATS,
    SIGNAL_NEW_STATS,

    /* FILL ME */
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_CAPS,
    PROP_PORT,
    PROP_WINDOW
};


/* the capabilities of the inputs and outputs.
 */
static GstStaticPadTemplate src_template = 
    GST_STATIC_PAD_TEMPLATE("src",
       GST_PAD_SRC,
       GST_PAD_ALWAYS,
       GST_STATIC_CAPS_ANY);

#define gst_prrt_source_parent_class parent_class

G_DEFINE_TYPE (GstPrrtSource, gst_prrt_source, GST_TYPE_PUSH_SRC);

static void gst_prrt_source_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static void gst_prrt_source_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static gboolean gst_prrt_source_src_query(GstPad *pad, GstObject *parent, GstQuery *query);

static GstCaps *gst_prrt_source_getcaps(GstBaseSrc *src, GstCaps *filter);

static GstFlowReturn gst_prrt_source_alloc(GstPushSrc *psrc, GstBuffer **buf);

static GstFlowReturn gst_prrt_source_fill(GstPushSrc *psrc, GstBuffer *buf);

static GstFlowReturn gst_prrt_source_create(GstPushSrc *psrc, GstBuffer **buf);

static GstStateChangeReturn gst_prrt_source_change_state(GstElement *element, GstStateChange transition);

static gboolean gst_prrt_source_open(GstPrrtSource *prrt_source);

static guint gst_prrt_source_signals[LAST_SIGNAL] = {0};

static gboolean gst_prrt_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_prrt_src_unlock_stop (GstBaseSrc * bsrc);

static GstBuffer *gst_prrt_source_deliver_buffer(GstPushSrc *psrc, gchar *data, gsize size);

static void gst_prrt_source_class_init(GstPrrtSourceClass *klass) {
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseSrcClass *gstbasesrc_class;
    GstPushSrcClass *gstpushsrc_class;

    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    gstbasesrc_class = (GstBaseSrcClass *) klass;
    gstpushsrc_class = (GstPushSrcClass *) klass;

    gobject_class->set_property = gst_prrt_source_set_property;
    gobject_class->get_property = gst_prrt_source_get_property;

    gst_prrt_source_signals[SIGNAL_GET_STATS] =
            g_signal_new("get-stats", G_TYPE_FROM_CLASS (klass),
                         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                         G_STRUCT_OFFSET (GstPrrtSourceClass, get_stats),
                         NULL, NULL, g_cclosure_marshal_generic, GST_TYPE_STRUCTURE, 0);

    gst_prrt_source_signals[SIGNAL_NEW_STATS] =
            g_signal_new("new-stats", G_TYPE_FROM_CLASS (klass),
                         G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstPrrtSourceClass, new_stats),
                         NULL, NULL, g_cclosure_marshal_generic, G_TYPE_NONE, 2,
                         G_TYPE_UINT64, G_TYPE_UINT64);

    g_object_class_install_property(gobject_class, PROP_CAPS,
                                    g_param_spec_boxed("caps", "Caps",
                                                       "The caps of the source pad", GST_TYPE_CAPS,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_PORT,
                                    g_param_spec_uint("port", "port", "The port",
                                                      0, 65535, PRRT_DEFAULT_PORT,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_WINDOW,
                                    g_param_spec_uint("window", "window", "The window",
                                                      0, 4294967295, PRRT_DEFAULT_WINDOW,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_element_class_set_details_simple(gstelement_class,
                                         "PRRT packet receiver",
                                         "Source/Network",
                                         "Receive data over network via PRRT",
                                         "Andreas Schmidt <schmidt@nt.uni-saarland.de>");

    gst_element_class_add_static_pad_template(gstelement_class, &src_template);
    
    gstelement_class->change_state = gst_prrt_source_change_state;
    
    gstbasesrc_class->unlock = gst_prrt_src_unlock;
    gstbasesrc_class->unlock_stop = gst_prrt_src_unlock_stop;
    gstbasesrc_class->get_caps = gst_prrt_source_getcaps;

    gstpushsrc_class->alloc = gst_prrt_source_alloc;
    gstpushsrc_class->fill = gst_prrt_source_fill;
    gstpushsrc_class->create = gst_prrt_source_create;

    klass->get_stats = gst_prrt_source_get_stats;
}

static void gst_prrt_source_init(GstPrrtSource *prrt_source) {
    prrt_source->port = PRRT_DEFAULT_PORT;
    prrt_source->window = PRRT_DEFAULT_WINDOW;
    prrt_source->caps_received = FALSE;

    gst_pad_set_query_function (GST_BASE_SRC_PAD(prrt_source), gst_prrt_source_src_query);

    /* configure basesrc to be a live source */
    gst_base_src_set_live((GstBaseSrc *) prrt_source, TRUE);
    /* make basesrc output a segment in time */
    gst_base_src_set_format((GstBaseSrc *) prrt_source, GST_FORMAT_TIME);
    /* make basesrc set timestamps on outgoing buffers based on the running_time
     * when they were captured */
    gst_base_src_set_do_timestamp((GstBaseSrc *) prrt_source, TRUE);
}

static GstCaps *gst_prrt_source_getcaps(GstBaseSrc *src, GstCaps *filter) {
    GstPrrtSource *prrt_source;
    GstCaps *caps, *result;

    prrt_source = GST_PRRTSRC (src);

    GST_OBJECT_LOCK (src);
    if ((caps = prrt_source->caps)) {
        gst_caps_ref(caps);
    }

    GST_OBJECT_UNLOCK (src);

    if (caps) {
        if (filter) {
            result = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
            gst_caps_unref(caps);
        } else {
            result = caps;
        }
    } else {
        result = (filter) ? gst_caps_ref(filter) : gst_caps_new_any();
    }
    return result;
}

static gboolean gst_prrt_source_src_query(GstPad *pad, GstObject *parent, GstQuery *query) {
    GstPrrtSource *prrt_source = GST_PRRTSRC (parent);
    gboolean ret = TRUE;

    /* Query types: DURATION, POSITION, SEEKING, URI */
    switch (GST_QUERY_TYPE(query)) {
        case GST_QUERY_LATENCY:
            gst_query_set_latency(query, TRUE, 0, 200000);
            break;
        default:
            ret = gst_pad_query_default(pad, parent, query);
            break;
    }

    return ret;
}

static void gst_prrt_source_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstPrrtSource *prrt_source = GST_PRRTSRC (object);

    switch (prop_id) {
        case PROP_CAPS: {
            const GstCaps *new_caps_val = gst_value_get_caps(value);
            GstCaps *new_caps;
            GstCaps *old_caps;

            if (new_caps_val == NULL) {
                new_caps = gst_caps_new_any();
            } else {
                new_caps = gst_caps_copy(new_caps_val);
            }

            GST_OBJECT_LOCK (prrt_source);
            old_caps = prrt_source->caps;
            prrt_source->caps = new_caps;
            GST_OBJECT_UNLOCK (prrt_source);
            if (old_caps) {
                gst_caps_unref(old_caps);
            }

            gst_pad_mark_reconfigure(GST_BASE_SRC_PAD (prrt_source));
            break;
        }
        case PROP_PORT:
            GST_DEBUG("Set port to %u", g_value_get_uint(value));
            prrt_source->port = g_value_get_uint(value);
            break;
        case PROP_WINDOW:
            GST_DEBUG("Set window to %u", g_value_get_uint(value));
            prrt_source->window = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void gst_prrt_source_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstPrrtSource *prrt_source = GST_PRRTSRC (object);

    switch (prop_id) {
        case PROP_CAPS:
            GST_OBJECT_LOCK (prrt_source);
            gst_value_set_caps(value, prrt_source->caps);
            GST_OBJECT_UNLOCK (prrt_source);
            break;
        case PROP_PORT:
            g_value_set_uint(value, prrt_source->port);
            break;
        case PROP_WINDOW:
            g_value_set_uint(value, prrt_source->window);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_prrt_src_unlock (GstBaseSrc * bsrc)
{
  GstPrrtSource *src;
  src = GST_PRRTSRC (bsrc);
  GST_LOG_OBJECT (src, "Flushing");
  
  return TRUE;
}

static gboolean
gst_prrt_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstPrrtSource *src;
  src = GST_PRRTSRC (bsrc);
  GST_LOG_OBJECT (src, "No longer flushing");

  return TRUE;
}

// Allocate memory and append to buffer
static GstFlowReturn gst_prrt_source_alloc(GstPushSrc *psrc, GstBuffer **buf) {
    GstPrrtSource *prrt_source;
    prrt_source = GST_PRRTSRC (psrc);

    GST_DEBUG("Allocating %u bytes...", prrt_source->total_size);
    
    GstMemory *mem;
    mem = gst_allocator_alloc(NULL, prrt_source->total_size, NULL);
    gst_buffer_append_memory(*buf, mem);
    
    return GST_FLOW_OK;
}

// Fill data received from PRRt to buffer
static GstFlowReturn gst_prrt_source_fill(GstPushSrc *psrc, GstBuffer *buf) {
    GstPrrtSource *prrt_source;
    prrt_source = GST_PRRTSRC (psrc);
    GstMapInfo info;
    
    gst_buffer_map(buf, &info, GST_MAP_READWRITE);
    
    memcpy(info.data, prrt_source->data, info.size);
    
    if (info.size) {
        GST_DEBUG("info.size: %d", info.size);
    }
    if (info.size) {
        GST_DEBUG("info.data:\n %s", info.data);
    }
    
    gst_buffer_unmap(buf, &info);
    
    free(prrt_source->data);

    return GST_FLOW_OK;
}

static GstFlowReturn gst_prrt_source_create(GstPushSrc *psrc, GstBuffer **buf) {
    GstPrrtSource *prrt_source = GST_PRRTSRC (psrc);
    GST_DEBUG("initial caps_received: %d", prrt_source->caps_received);
    GError *error = NULL;

    // set caps
    if(prrt_source->caps_received == FALSE) {
        prrt_source->caps_received = TRUE;
        char *buffer = malloc((MAX_PAYLOAD_LENGTH + 1) * sizeof(char));

        struct sockaddr_in addr;
        int n = PrrtSocket_recv(prrt_source->recv_socket, buffer, &addr);
        //int n = PrrtSocket_receive_ordered_wait(prrt_source->recv_socket, buffer, &addr, prrt_source->window);
        GST_DEBUG("Received data from prrt: %d", n);

        if (n <= 0) {
            GST_DEBUG("Socket receiving error");
            return GST_FLOW_ERROR;
        }
        
        // reallloc buffer to n (bytes)
        buffer = realloc(buffer, n);
        
        GstCaps *negotiated_caps = gst_caps_from_string(buffer);
        prrt_source->caps = negotiated_caps;
        
        // reconfigure cap
        gst_pad_mark_reconfigure(GST_BASE_SRC_PAD (prrt_source));
        GST_DEBUG("set caps to: %s", buffer);

        // free allocated buffer
        if (buffer) {
            free(buffer);
        }
    }

    // loop to wait for incomming data
    while (true) {
        GST_DEBUG("Loop start");
        char *buffer = (char *) calloc(1, MAX_PAYLOAD_LENGTH + 1);
        struct sockaddr_in addr;
        
        // poll data from prrt socket
        int n = PrrtSocket_receive_ordered_wait(prrt_source->recv_socket, buffer, &addr, prrt_source->window);

        if (n > 0) {
            GST_DEBUG("Received Something. Processing...");
            prrt_source->bytes_received += n;
            prrt_source->packets_received++;
            GST_DEBUG("Received data size: %u", n);

            // call functions to allocate memory and fill
            *buf = gst_prrt_source_deliver_buffer(psrc, buffer, n);
            
            // free allocated buffer
            if (buffer) {
                free(buffer);
            }
            
            return (*buf == NULL) ? GST_FLOW_ERROR : GST_FLOW_OK;
        }
        
    }

    // allocate new memory for gst_buf
    GstBuffer *gst_buf;
    gst_buf = gst_buffer_new();
    GstFlowReturn ret;
    
    ret = gst_prrt_source_alloc(psrc, &gst_buf);
    if (ret != GST_FLOW_OK) {
        GST_DEBUG ("Failed to allocate buffer");
        return ret;
    }
    GST_DEBUG("Allocation successfull.");
    
    // Fill data to gst_buf
    ret = gst_prrt_source_fill(psrc, gst_buf);
    if (ret != GST_FLOW_OK) {
        GST_DEBUG ("Failed to fill buffer");
        return ret;
    }
    GST_DEBUG("Filled data to buffer successfully");

    *buf = gst_buf;
    
    return (*buf == NULL) ? GST_FLOW_ERROR : GST_FLOW_OK;

}

static GstBuffer * gst_prrt_source_deliver_buffer(GstPushSrc *psrc, gchar *data, gsize size) {     
    GstPrrtSource *prrt_source = GST_PRRTSRC (psrc);     
    GstFlowReturn ret;     
    GstBuffer *buf; 
        
    prrt_source->total_size = size;     
    prrt_source->data = malloc(size); 
        
    memcpy(prrt_source->data, data, size); 
          
    buf = gst_buffer_new();     
    ret = gst_prrt_source_alloc(psrc, &buf);
    
    if (ret != GST_FLOW_OK) {         
        goto alloc_failed;
    }
         
    GST_DEBUG("Allocation successfull.");
         
    ret = gst_prrt_source_fill(psrc, buf);     
    
    if (ret != GST_FLOW_OK) {
        goto fill_failed;
    }     
    
    GST_DEBUG("Fill successfull.");     
    return buf;     

alloc_failed:     
    GST_DEBUG ("Failed to allocate buffer");     
    return NULL;     
fill_failed:     
    GST_DEBUG ("Failed to fill buffer");     
    return NULL; 
}

static gboolean prrtsrc_init(GstPlugin *prrtsrc) {
    GST_DEBUG_CATEGORY_INIT(gst_prrt_source_debug, "prrtsrc",
                            0, "Template prrtsrc");

    return gst_element_register(prrtsrc, "prrtsrc", GST_RANK_NONE,
                                GST_TYPE_PRRTSRC);
}

static gboolean gst_prrt_source_open(GstPrrtSource *prrt_source) {
    GST_DEBUG ("opening socket");
    prrt_source->recv_socket = PrrtSocket_create(5000, 500000);
    GST_DEBUG("Binding to port: %u", prrt_source->port);
    int bind_var=PrrtSocket_bind(prrt_source->recv_socket, "0.0.0.0", prrt_source->port);
    if (bind_var != 0) {
        GST_ERROR_OBJECT(prrt_source, "Receive Socket binding failed: %d.\n", bind_var);
        return FALSE;
    }
    if (prrt_source->recv_socket == NULL) {
        GST_ERROR_OBJECT(prrt_source, "socket creation failed.\n");
        return FALSE;
    }
    
    prrt_debug_log = fopen("~/prrt_debug_log.txt", "w+");
    
    return TRUE;
}

static gboolean gst_prrt_source_close(GstPrrtSource *prrt_source) {
    GST_DEBUG ("closing socket");
    if (prrt_source->recv_socket) {
        PrrtSocket_close(prrt_source->recv_socket);
        prrt_source->recv_socket = NULL;
    }
    
    fclose(prrt_debug_log);

    return TRUE;
}

static GstStateChangeReturn gst_prrt_source_change_state(GstElement *element, GstStateChange transition) {
    GstPrrtSource *prrt_source;
    GstStateChangeReturn result;

    prrt_source = GST_PRRTSRC (element);

    switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY:
            if (!gst_prrt_source_open(prrt_source))
                goto open_failed;
            break;
        default:
            break;
    }
    if ((result =
                 GST_ELEMENT_CLASS (parent_class)->change_state(element,
                                                                transition)) == GST_STATE_CHANGE_FAILURE)
        goto failure;

    switch (transition) {
        case GST_STATE_CHANGE_READY_TO_NULL:
            gst_prrt_source_close(prrt_source);
            break;
        default:
            break;
    }
    return result;
    /* ERRORS */
    open_failed:
    {
        GST_DEBUG_OBJECT (prrt_source, "failed to open socket");
        return GST_STATE_CHANGE_FAILURE;
    }
    failure:
    {
        GST_DEBUG_OBJECT (prrt_source, "parent failed state change");
        return result;
    }
}

GstStructure * gst_prrt_source_get_stats(GstPrrtSource *source) {
    GstStructure *result = NULL;

    result = gst_structure_new_empty("prrtsrc-stats");

    gst_structure_set(result,
                      "bytes-received", G_TYPE_UINT64, source->bytes_received,
                      "packets-received", G_TYPE_UINT64, source->packets_received, NULL);

    return result;
}

#ifndef PACKAGE
#define PACKAGE "gst-prrt"
#endif

#ifndef VERSION
#define VERSION "1.0"
#endif

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    prrtsrc,
    "a plugin transfers data using PRRT protocol",
    plugin_init,
    VERSION,
    "MIT/X11",
    "none",
    "https://github.com/hieunk58/prrt_plugin"
)

