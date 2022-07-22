#include "gstprrtsink.h"

#define PRRT_SINK_DEFAULT_PORT 5000
#define PRRT_SINK_DEFAULT_HOST "127.0.0.1"

static GstStaticPadTemplate sink_factory =
GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

static void gst_prrtsink_class_init (GstPRRTSinkClass *klass);
static void gst_prrtsink_init (GstPRRTSink *prrtsink);

static void gst_prrtsink_class_init (GstPRRTSinkClass *klass) {
    GST_DEBUG ("gst_prrtsink_class_init");

    GObjectClass        *gobject_class;
    GstElementClass     *gstelement_class;
    GstBaseSinkClass    *gstbase_sink_class;

    gobject_class       = (GObjectClass *) klass;
    gstelement_class    = (GstElementClass *) klass;
    gstbase_sink_class  = GST_BASE_SINK_CLASS (klass);

    //gobject_class->set_property = gst_prrtsink_set_property;
    //gobject_class->get_property = gst_prrtsink_get_property;

    // TODO install property

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