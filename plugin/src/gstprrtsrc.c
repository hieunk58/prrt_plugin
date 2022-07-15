#include "gstprrtsrc.h"

static GstStaticPadTemplate src_factory =
GST_STATIC_PAD_TEMPLATE (
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

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
