/*
 * GStreamer
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
#include <config.h>
#endif

#include <gst/gst.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "gstprrtsink.h"

#define PRRT_DEFAULT_HOST "127.0.0.1"
#define PRRT_DEFAULT_PORT 5000
#define PRRT_DEFAULT_LOCALPORT 6000
#define PRRT_DEFAULT_TARGETDELAY 2000 * 1000
#define PRRT_DEFAULT_MAXBUFFERSIZE 1400
#define PRRT_DEFAULT_CODINGCONFIG "1,1,[]"

GST_DEBUG_CATEGORY_STATIC(gst_prrt_sink_debug);
#define GST_CAT_DEFAULT gst_prrt_sink_debug

enum
{
    PROP_0,
    PROP_SILENT,
    PROP_HOST,
    PROP_PORT,
    PROP_LOCALPORT,
    PROP_TARGETDELAY,
    PROP_MAXBUFFERSIZE,
    PROP_CODINGCONFIG
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE("sink",
                                                                    GST_PAD_SINK,
                                                                    GST_PAD_ALWAYS,
                                                                    GST_STATIC_CAPS_ANY);

#define gst_prrt_sink_parent_class parent_class

G_DEFINE_TYPE(GstPrrtSink, gst_prrt_sink, GST_TYPE_BASE_SINK);

static void gst_prrt_sink_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static void gst_prrt_sink_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static gboolean gst_prrt_sink_sink_event(GstPad *pad, GstObject *parent, GstEvent *event);

static GstFlowReturn gst_prrt_sink_render(GstBaseSink *parent_sink, GstBuffer *buffer);

static gboolean gst_prrt_sink_start(GstBaseSink *bsink);

static gboolean gst_prrt_sink_stop(GstBaseSink *bsink);

static struct CodingConfiguration *gst_prrt_sink_read_codingconfig(char *conf_string);

static void gst_prrt_sink_class_init(GstPrrtSinkClass *klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseSinkClass *gstbase_sink_class;

    gobject_class = (GObjectClass *)klass;
    gstelement_class = (GstElementClass *)klass;
    gstbase_sink_class = GST_BASE_SINK_CLASS(klass);

    gobject_class->set_property = gst_prrt_sink_set_property;
    gobject_class->get_property = gst_prrt_sink_get_property;

    g_object_class_install_property(gobject_class, PROP_SILENT,
                                    g_param_spec_boolean("silent", "Silent", "Produce verbose output ?",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_HOST,
                                    g_param_spec_string("host", "host", "The host address to send the packets to",
                                                        PRRT_DEFAULT_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_PORT,
                                    g_param_spec_uint("port", "port", "The port to send the packets to",
                                                      0, 65535, PRRT_DEFAULT_PORT,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_LOCALPORT,
                                    g_param_spec_uint("localport", "localport", "The local port to bind the socket to",
                                                      0, 65535, PRRT_DEFAULT_LOCALPORT,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_TARGETDELAY,
                                    g_param_spec_uint("targetdelay", "targetdelay", "The target delay of the socket",
                                                      0, 4294967295, PRRT_DEFAULT_TARGETDELAY,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_MAXBUFFERSIZE,
                                    g_param_spec_uint("maxbuffersize", "maxbuffersize", "The maximum buffer size to be send",
                                                      0, 4294967295, PRRT_DEFAULT_MAXBUFFERSIZE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_CODINGCONFIG,
                                    g_param_spec_string("codingconfig", "codingconfig", "The codingconfig",
                                                        PRRT_DEFAULT_CODINGCONFIG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_element_class_set_static_metadata(gstelement_class,
                                          "PRRT packet sender", "Source/Network",
                                          "Send data over the network via PRRT",
                                          "Hieu Nguyen <khachieunk@gmail.com>");

    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&sink_template));
    gstbase_sink_class->render = GST_DEBUG_FUNCPTR(gst_prrt_sink_render);
    gstbase_sink_class->start = gst_prrt_sink_start;
    gstbase_sink_class->stop = gst_prrt_sink_stop;
}

static void gst_prrt_sink_init(GstPrrtSink *prrt_sink)
{
    prrt_sink->silent = FALSE;
    prrt_sink->host = PRRT_DEFAULT_HOST;
    prrt_sink->port = PRRT_DEFAULT_PORT;
    prrt_sink->localport = PRRT_DEFAULT_LOCALPORT;
    prrt_sink->targetdelay = PRRT_DEFAULT_TARGETDELAY;
    prrt_sink->maxbuffersize = PRRT_DEFAULT_MAXBUFFERSIZE;
    prrt_sink->codingconfig = PRRT_DEFAULT_CODINGCONFIG;
}

static void gst_prrt_sink_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstPrrtSink *prrt_sink = GST_PRRTSINK(object);

    switch (prop_id)
    {
    case PROP_SILENT:
        prrt_sink->silent = g_value_get_boolean(value);
        break;
    case PROP_HOST:
        prrt_sink->host = g_strdup(g_value_get_string(value));
        break;
    case PROP_PORT:
        prrt_sink->port = g_value_get_uint(value);
        break;
    case PROP_LOCALPORT:
        prrt_sink->localport = g_value_get_uint(value);
        break;
    case PROP_TARGETDELAY:
        prrt_sink->targetdelay = g_value_get_uint(value);
        break;
    case PROP_MAXBUFFERSIZE:
        prrt_sink->maxbuffersize = g_value_get_uint(value);
        break;
    case PROP_CODINGCONFIG:
        prrt_sink->codingconfig = g_strdup(g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_prrt_sink_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstPrrtSink *prrtsink = GST_PRRTSINK(object);

    switch (prop_id)
    {
    case PROP_SILENT:
        g_value_set_boolean(value, prrtsink->silent);
        break;
    case PROP_HOST:
        g_value_set_string(value, prrtsink->host);
        break;
    case PROP_PORT:
        g_value_set_uint(value, prrtsink->port);
        break;
    case PROP_LOCALPORT:
        g_value_set_uint(value, prrtsink->localport);
        break;
    case PROP_TARGETDELAY:
        g_value_set_uint(value, prrtsink->targetdelay);
        break;
    case PROP_MAXBUFFERSIZE:
        g_value_set_uint(value, prrtsink->maxbuffersize);
        break;
    case PROP_CODINGCONFIG:
        g_value_set_string(value, prrtsink->codingconfig);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean gst_prrt_sink_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    GstPrrtSink *prrt_sink;
    gboolean ret;

    prrt_sink = GST_PRRTSINK(parent);

    GST_LOG_OBJECT(prrt_sink, "Received %s event: %s (%p)", GST_PTR_FORMAT, GST_EVENT_TYPE_NAME(event), event);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;

        gst_event_parse_caps(event, &caps);
        gchar *caps_description = gst_caps_to_string(caps);
        GST_DEBUG_OBJECT(prrt_sink, "caps: %s", caps_description);
        ret = gst_pad_event_default(pad, parent, event);
        break;
    }
    case GST_EVENT_EOS:
    {
        GST_DEBUG("GST_EVENT_EOS");
        ret = TRUE;
        break;
    }
    default:
        ret = gst_pad_event_default(pad, parent, event);
        break;
    }
    return ret;
}

gboolean negotiate_caps = TRUE;

static GstFlowReturn gst_prrt_sink_render(GstBaseSink *parent_sink, GstBuffer *buffer)
{
    GstPrrtSink *prrt_sink;
    prrt_sink = GST_PRRTSINK(parent_sink);

    /* Use this to find out the capabilities of the stream,
    They need to be sent to the receiver. TODO: Sometimes this is NULL*/
    if (negotiate_caps)
    {
        GstCaps *caps = gst_pad_get_current_caps(GST_BASE_SINK_PAD(prrt_sink));
        if (caps != NULL)
        {
            gchar *description = gst_caps_to_string(caps);
            GST_DEBUG("%s", description);
            PrrtSocket_send_sync(prrt_sink->sender_socket, (const uint8_t *)description, strlen(description));
            negotiate_caps = FALSE;
        }
        else
        {
            GST_DEBUG("Get caps failed");
            PrrtSocket_send_sync(prrt_sink->sender_socket, NULL, 0);
            negotiate_caps = FALSE;
        }
    }

    /* Prepare reading the buffer content */
    GstMapInfo info;
    if (!gst_buffer_map(buffer, &info, GST_MAP_READ))
    {
        GST_ERROR_OBJECT(prrt_sink, "gst_buffer_map error ");
        return GST_FLOW_ERROR;
    }
    GST_LOG("Data size: %lu", info.size);

    /* Split up the buffer in prrt_sink->maxbuffersize sized segments (MTU optmized) */
    gsize temp_size = info.size;
    guint8 *temp_data = info.data;
    while (temp_size > prrt_sink->maxbuffersize)
    {
        PrrtSocket_send_sync(prrt_sink->sender_socket, temp_data, prrt_sink->maxbuffersize);
        GST_LOG("Sent buffer size: %d", prrt_sink->maxbuffersize);
        GST_DEBUG("Paces: %d app_send, %d transmit, %d total",
                  PrrtSocket_get_sock_opt(prrt_sink->sender_socket, "appSend_pace_effective"),
                  PrrtSocket_get_sock_opt(prrt_sink->sender_socket, "prrtTransmit_pace_effective"),
                  PrrtSocket_get_sock_opt(prrt_sink->sender_socket, "appSend_pace_total"));
        temp_data += prrt_sink->maxbuffersize;
        temp_size -= prrt_sink->maxbuffersize;
    }
    PrrtSocket_send_sync(prrt_sink->sender_socket, temp_data, temp_size);
    GST_LOG("Sent buffer size: %lu", temp_size);
    gst_buffer_unmap(buffer, &info);
    return GST_FLOW_OK;
}

static gboolean prrtsink_init(GstPlugin *prrtsink)
{
    GST_LOG("Init prrtsink\n");
    return gst_element_register(prrtsink, "prrtsink",
                                GST_RANK_NONE,
                                GST_TYPE_PRRTSINK);
}

static gboolean gst_prrt_sink_start(GstBaseSink *bsink)
{
    GstPrrtSink *prrt_sink;
    prrt_sink = GST_PRRTSINK(bsink);

    prrt_sink->sender_socket = PrrtSocket_create(prrt_sink->maxbuffersize, prrt_sink->targetdelay);

    prrt_sink->sender_socket->pacingEnabled = false;

    struct CodingConfiguration *conf;
    conf = gst_prrt_sink_read_codingconfig(prrt_sink->codingconfig);
    PrrtSocket_set_coding_parameters(prrt_sink->sender_socket, conf->k, conf->n, conf->c, conf->n_cycle);
    if (conf->n_cycle != NULL)
    {
        free(conf->n_cycle);
    }
    free(conf);

    int bind_var = PrrtSocket_bind(prrt_sink->sender_socket, "0.0.0.0", prrt_sink->localport);
    if (bind_var != 0)
    {
        GST_ERROR_OBJECT(prrt_sink, "Receive Socket binding failed: %d.\n", bind_var);
        return FALSE;
    }
    if (prrt_sink->sender_socket == NULL)
    {
        GST_ERROR("socket creation failed.");
        return FALSE;
    }
    PrrtSocket_connect(prrt_sink->sender_socket, prrt_sink->host, prrt_sink->port);
    return TRUE;
}

static gboolean gst_prrt_sink_stop(GstBaseSink *bsink)
{
    GstPrrtSink *prrt_sink;

    prrt_sink = GST_PRRTSINK(bsink);

    PrrtSocket_close(prrt_sink->sender_socket);
    free(prrt_sink->sender_socket);
    free(prrt_sink->host);
    free(prrt_sink->codingconfig);

    return TRUE;
}

static struct CodingConfiguration *gst_prrt_sink_read_codingconfig(char *conf_string)
{
    char *string = g_strdup(conf_string);

    uint8_t count = 0;
    for (int m = 0; string[m]; m++)
    {
        if (string[m] == ',')
        {
            count++;
        }
    }
    if (count < 2)
    {
        return NULL;
    }
    count = count - 1;
    uint8_t *list = NULL;

    char *pt;
    pt = strtok(string, ",");
    uint8_t n = atoi(pt);
    pt = strtok(NULL, ",");
    uint8_t k = atoi(pt);
    pt = strtok(NULL, "[,]");

    if (n == k)
    {
        count = 0;
    }
    else
    {
        list = malloc(count * sizeof(uint8_t));
        uint8_t c = 0;
        while (pt != NULL && c < count)
        {
            list[c] = atoi(pt);
            pt = strtok(NULL, ",]");
            c++;
        }
    }

    free(string);

    struct CodingConfiguration *conf = malloc(sizeof(struct CodingConfiguration));
    conf->n = n;
    conf->k = k;
    conf->c = count;
    conf->n_cycle = list;

    return conf;
}

#ifndef PACKAGE
#define PACKAGE "gst-prrt"
#endif

#ifndef VERSION
#define VERSION "1.0"
#endif

GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    prrtsink,
    "a plugin transfers data using PRRT protocol",
    prrtsink_init,
    VERSION,
    "MIT",
    "none",
    "https://github.com/hieunk58/prrt_plugin")
