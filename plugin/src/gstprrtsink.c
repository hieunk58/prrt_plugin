#include "gstprrtsink.h"

#define PRRT_SINK_DEFAULT_PORT 5000
#define PRRT_SINK_DEFAULT_HOST "127.0.0.1"
#define PRRT_SINK_DEFAULT_MAX_BUFF_SIZE 1400
#define PRRT_SINK_DEFAULT_TARGET_DELAY 2000*1000
#define PRRT_SINK_DEFAULT_HEADER_SIZE 7

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
static gboolean gst_prrtsink_start(GstBaseSink *bsink);
static gboolean gst_prrtsink_stop(GstBaseSink *bsink);
static GstFlowReturn gst_prrtsink_render (GstBaseSink *bsink, GstBuffer *buffer);

#define gst_prrtsink_parent_class parent_class
G_DEFINE_TYPE (GstPRRTSink, gst_prrtsink, GST_TYPE_BASE_SINK);

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

    gstbase_sink_class->render = gst_prrtsink_render;
    gstbase_sink_class->start = gst_prrtsink_start;
    gstbase_sink_class->stop = gst_prrtsink_stop;
}

static void gst_prrtsink_init (GstPRRTSink *prrtsink) {
    prrtsink->host = PRRT_SINK_DEFAULT_HOST;
    prrtsink->port = PRRT_SINK_DEFAULT_PORT;
    prrtsink->max_buff_size = PRRT_SINK_DEFAULT_MAX_BUFF_SIZE;
    prrtsink->target_delay = PRRT_SINK_DEFAULT_TARGET_DELAY;
    prrtsink->negotiate_cap = TRUE;

    prrtsink->frame_size = 0;
    prrtsink->header_frame = 0;
    prrtsink->header_seq = 0;
    prrtsink->header_size = PRRT_SINK_DEFAULT_HEADER_SIZE;

    prrtsink->packet_data_size = prrtsink->max_buff_size - prrtsink->header_size;
    prrtsink->tmp_packet = calloc(PRRT_SINK_DEFAULT_MAX_BUFF_SIZE, sizeof(guint8));
}

// set header for the packet
// frame nr 1 byte
// seq nr 2 bytes
// total size 4 bytes
void set_header(guint8* arr, guint8 frame_nr, guint16 seq_nr, guint32 total_size) {
    // frame number
    arr[0] = frame_nr;
    // seq number
    arr[1] = (seq_nr >> 8);
    arr[2] = (seq_nr << 8) >> 8;
    // total size
    arr[3] = total_size >> 24;
    arr[4] = (total_size << 8) >> 24;
    arr[5] = (total_size << 16) >> 24;
    arr[6] = (total_size << 24) >> 24;
}

static GstFlowReturn gst_prrtsink_render (GstBaseSink *bsink, GstBuffer *buffer) {
    GstPRRTSink *sink;
    GstFlowReturn flow;

    sink = GST_PRRTSINK (bsink);

    // get stream capabilities
    if(sink->negotiate_cap) {
        GstCaps *caps = gst_pad_get_current_caps (GST_BASE_SINK_PAD (sink));
        if (caps != NULL) {
            gchar* description = gst_caps_to_string (caps);
            GST_DEBUG ("current caps: %s", description);
            PrrtSocket_send_sync (sink->used_socket, (const uint8_t*) description, 
                strlen (description));
            sink->negotiate_cap = FALSE;
        } else {
            GST_DEBUG ("Error! Could not get caps");
            PrrtSocket_send_sync (sink->used_socket, NULL, 0);
            sink->negotiate_cap = FALSE;
        }
    }

    // reading buffer content
    GstMapInfo info;
    if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
        GST_ERROR_OBJECT(sink, "Getting buffer map error!!");
        return GST_FLOW_ERROR;
    }
    GST_LOG("Data size: %lu", info.size);

    sink->frame_size = info.size;
    sink->header_seq = 0;
    gsize remain_size = info.size;
    guint8* remain_data = info.data;

    while (remain_size > sink->packet_data_size) {
        usleep (50);
        // building temp packet
        set_header (sink->tmp_packet, sink->header_frame, 
            ++sink->header_seq,
            sink->frame_size);
        memcpy (sink->tmp_packet + sink->header_size, 
            remain_data, sink->packet_data_size);

        // sending temp packet
        PrrtSocket_send_sync (sink->used_socket, sink->tmp_packet, 
            sink->max_buff_size);
        // update remain data
        remain_size -= sink->packet_data_size;
        // move pointer to the next data
        remain_data += sink->packet_data_size;
    }
    usleep (10000);

    // building and sending the last packet
    set_header (sink->tmp_packet, sink->header_frame, 
        sink->header_seq, sink->frame_size);
    memcpy (sink->tmp_packet + sink->header_size, 
        remain_data, remain_size);
    PrrtSocket_send_sync (sink->used_socket, sink->tmp_packet, 
        remain_size + sink->header_size);

    // increase frame number
    ++sink->header_frame;

    gst_buffer_unmap (buffer, &info);
    return GST_FLOW_OK
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

static gboolean gst_prrtsink_start(GstBaseSink *bsink) {
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

static gboolean gst_prrtsink_stop(GstBaseSink *bsink) {
    GstPRRTSink *sink;
    sink = GST_PRRTSINK (bsink);

    PrrtSocket_close (sink->used_socket);
    free (sink->used_socket);
    free (sink->host);

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