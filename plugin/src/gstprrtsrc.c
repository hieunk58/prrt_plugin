#include "gstprrtsrc.h"

static void gst_prrtsrc_class_init (GstPRRTSrcClass *klass);

static GstStaticPadTemplate src_factory =
GST_STATIC_PAD_TEMPLATE (
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

/* _class_init() is used to initialized the prrtsrc class only once
* (specifying what signals, arguments and virtual functions the class has
* and setting up global state)
*/
static void gst_prrtsrc_class_init (GstPRRTSrcClass *klass) {
    GST_DEBUG ("gst_prrtsrc_class_init");
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    // add pad template
    gst_element_class_add_static_pad_template (element_class, &src_factory);
}

/* plugin_init is a special function which is called
* as soon as the plugin is loaded
*/
static gboolean plugin_init (GstPlugin *prrtsrc) {
    return gst_element_register(prrtsrc, "prrtsrc",
        GST_RANK_NONE,
        GST_TYPE_PRRTSRC);
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
    "MIT",
    "none",
    "https://github.com/hieunk58/prrt_plugin"
)
