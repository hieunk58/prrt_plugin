#include "gstprrtsrc.h"

#include <gst/gst.h>

static void gst_prrtsrc_class_init (GstPRRTSrcClass *klass);
static void gst_prrtsrc_init (GstPRRTSrc *prrt_src);
static gboolean gst_prrtsrc_src_query (GstPad *pad, GstObject *parent, 
    GstQuery *query);
static GstStateChangeReturn
gst_prrtsrc_change_state (GstElement *element, GstStateChange transition);

static GstStaticPadTemplate src_factory =
GST_STATIC_PAD_TEMPLATE (
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

#define gst_prrtsrc_parent_class parent_class
G_DEFINE_TYPE (GstPRRTSrc, gst_prrtsrc, GST_TYPE_PUSH_SRC);

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

/* queries like position, duration, supported formats
* queries can be both upstream and downstream
*/
static gboolean 
gst_prrtsrc_src_query (GstPad *pad, GstObject *parent, GstQuery *query) {
    gboolean ret = TRUE;
    switch (GST_QUERY_TYPE (query))
    {
    case GST_QUERY_POSITION:
        /* report the current position */     
        break;
    case GST_QUERY_DURATION:
        /* report the duration */     
        break;
    case GST_QUERY_CAPS:
        /* report the supported caps */     
        break;
    default:
        /* call the default handler */
        ret = gst_pad_query_default (pad, parent, query);
        break;
    }

    return ret;
}

static GstStateChangeReturn
gst_prrtsrc_change_state (GstElement *element, GstStateChange transition) {
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstPRRTSrc *src = GST_PRRTSRC (element);

    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
        // TODO call function to open socket for listening
        break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        goto failure;
    }

    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_NULL:
        // TODO close socket, free memory
        break;
    default:
        break;
    }

    return ret;

failure:
    {
        GST_DEBUG_OBJECT (src, "parent failed state change");
        return ret;
    }
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
