#include "gstprrtsink.h"

#define PRRT_SINK_DEFAULT_PORT 5000
#define PRRT_SINK_DEFAULT_HOST "127.0.0.1"
#define PRRT_SINK_DEFAULT_MAX_BUFF_SIZE 1400
#define PRRT_SINK_DEFAULT_TARGET_DELAY 2000*1000

// PROPERTIES
enum {
    PROP_0,

    PROP_HOST,
    PROP_PORT,
    PROP_TARGET_DELAY,
    PROP_MAX_BUFF_SIZE
};

static GstStaticPadTemplate sink_factory =
GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

static void gst_prrtsink_class_init (GstPRRTSinkClass *klass);
static void gst_prrtsink_init (GstPRRTSink *prrtsink);

static void gst_prrtsink_set_property(GObject *object, guint prop_id, 
    const GValue *value, GParamSpec *pspec);
static void gst_prrtsink_get_property(GObject *object, guint prop_id, 
    GValue *value, GParamSpec *pspec);
static gboolean gst_prrt_sink_start(GstBaseSink *bsink);

static void gst_prrtsink_class_init (GstPRRTSinkClass *klass) {
    GST_DEBUG ("gst_prrtsink_class_init");

    GObjectClass        *gobject_class;
    GstElementClass     *gstelement_class;
    GstBaseSinkClass    *gstbase_sink_class;

    gobject_class       = (GObjectClass *) klass;
    gstelement_class    = (GstElementClass *) klass;
    gstbase_sink_class  = GST_BASE_SINK_CLASS (klass);

    gobject_class->set_property = gst_prrtsink_set_property;
    gobject_class->get_property = gst_prrtsink_get_property;

    g_object_class_install_property(gobject_class, PROP_HOST,
        g_param_spec_string("host", "host", 
            "The host address to send the packets to",
            PRRT_SINK_DEFAULT_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_PORT,
        g_param_spec_uint("port", "port", 
            "The port to send the packets to",
            0, 65535, PRRT_SINK_DEFAULT_PORT,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    
    g_object_class_install_property(gobject_class, PROP_TARGET_DELAY,
        g_param_spec_uint("target_delay", "target_delay", 
            "The target delay of the socket in micro seconds",
            0, 4294967295, PRRT_SINK_DEFAULT_TARGET_DELAY,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_MAX_BUFF_SIZE,
        g_param_spec_uint("max_buff_size", "max_buff_size", 
            "The maximum buffer size to be sent",
            0, 4294967295, PRRT_SINK_DEFAULT_MAX_BUFF_SIZE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_element_class_set_static_metadata (gstelement_class,
        "PRRT packet sender", "Source/Network",
        "Send data over the network via PRRT",
        "Hieu Nguyen <khachieunk@gmail.com>");

    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&sink_factory));

    //gstbase_sink_class->render = gst_prrtsink_render;
    //gstbase_sink_class->start = gst_prrtsink_start;
    //gstbase_sink_class->stop = gst_prrtsink_stop;
}

static void gst_prrtsink_init (GstPRRTSink *prrtsink) {
    prrtsink->host = PRRT_SINK_DEFAULT_HOST;
    prrtsink->port = PRRT_SINK_DEFAULT_PORT;
    prrtsink->max_buff_size = PRRT_SINK_DEFAULT_MAX_BUFF_SIZE;
    prrtsink->target_delay = PRRT_SINK_DEFAULT_TARGET_DELAY;
}

static void gst_prrtsink_set_property(GObject *object, guint prop_id, 
    const GValue *value, GParamSpec *pspec) {
    GstPRRTSink *prrtsink = GST_PRRTSINK (object);

    switch (prop_id)
    {
    case PROP_HOST:
        const gchar *host = g_value_get_string (value);
        g_free (prrtsink->host);
        prrtsink->host = g_strdup (host);
        break;
    case PROP_PORT:
        prrtsink->port = g_value_get_uint (value);
        break;
    case PROP_MAX_BUFF_SIZE:
        prrtsink->max_buff_size = g_value_get_uint (value);
        break;
    case PROP_TARGET_DELAY:
        prrtsink->target_delay = g_value_get_uint (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_prrtsink_get_property(GObject *object, guint prop_id, 
    GValue *value, GParamSpec *pspec) {
    GstPRRTSink *prrtsink = GST_PRRTSINK (object);

    switch (prop_id)
    {
    case PROP_HOST:
        g_value_set_string (value, prrtsink->host);
        break;
    case PROP_PORT:
        g_value_set_uint (value, prrtsink->port);
        break;
    case PROP_MAX_BUFF_SIZE:
        g_value_set_uint (value, prrtsink->max_buff_size);
        break;
    case PROP_TARGET_DELAY:
        g_value_set_uint (value, prrtsink->target_delay);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean gst_prrt_sink_start(GstBaseSink *bsink) {
    GstPRRTSink *sink;
    sink = GST_PRRTSINK (bsink);

    // create a socket and set pacing to false
    sink->used_socket = PrrtSocket_create (sink->max_buff_size, 
                            sink->target_delay);
    sink->used_socket->pacingEnabled = false;

    // binding socket
    int res_binding = PrrtSocket_bind (sink->used_socket, "0.0.0.0", sink->port);
    if (res_binding != 0) {
        GST_ERROR_OBJECT(sink, "Socket binding failed: %d.\n", res_binding);
        return FALSE;
    }

    if (sink->used_socket = NULL) {
        GST_ERROR ("could not create socket");
        return FALSE;
    }

    int res_connect = PrrtSocket_connect (sink->used_socket, sink->host, sink->port);
    if (res_connect != 1) {
        GST_ERROR ("prrt socket connect failed");
        return FALSE;
    }

    return TRUE;
}

static gboolean prrtsink_init (GstPlugin *prrtsink) {
    return gst_element_register(prrtsink, "prrtsink",
        GST_RANK_NONE,
        GST_TYPE_PRRTSINK);
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
    prrtsink,
    "a plugin transfers data using PRRT protocol",
    prrtsink_init,
    VERSION,
    "MIT",
    "none",
    "https://github.com/hieunk58/prrt_plugin"
)