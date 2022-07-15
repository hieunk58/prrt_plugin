#include "gstprrtsrc.h"

static void gst_prrtsrc_class_init (GstPRRTSrcClass *klass);
static void gst_prrtsrc_init (GstPRRTSrc *prrt_src);

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

/* _init() function is used to initialize a specific instance of prrtsrc type */
static void gst_prrtsrc_init (GstPRRTSrc *prrt_src) {
    GST_DEBUG ("gst_prrtsrc_init");

    // TODO set some default values for plugin

    /* configure basesrc to be a live source */
  gst_base_src_set_live (GST_BASE_SRC (prrt_src), TRUE);
  /* make basesrc output a segment in time */
  gst_base_src_set_format (GST_BASE_SRC (prrt_src), GST_FORMAT_TIME);
  /* make basesrc set timestamps on outgoing buffers based on the running_time
   * when they were captured */
  gst_base_src_set_do_timestamp (GST_BASE_SRC (prrt_src), TRUE);
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
