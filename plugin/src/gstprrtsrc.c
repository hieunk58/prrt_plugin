#include "gstprrtsrc.h"

#include <gst/gst.h>

#define PRRT_DEFAULT_PORT 5000

/* PROPERTIES */
enum {
    PROP_0,

    PROP_PORT,
    PROP_CAPS
};

static void gst_prrtsrc_class_init (GstPRRTSrcClass *klass);
static void gst_prrtsrc_init (GstPRRTSrc *prrt_src);
static gboolean gst_prrtsrc_src_query (GstPad *pad, GstObject *parent, 
    GstQuery *query);
static GstStateChangeReturn
gst_prrtsrc_change_state (GstElement *element, GstStateChange transition);
static GstCaps *gst_prrtsrc_getcaps (GstBaseSrc *src, GstCaps *filter);
static void gst_prrtsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_prrtsrc_get_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

 
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
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseSrcClass *gstbasesrc_class;
    GstPushSrcClass *gstpushsrc_class;

    gobject_class    = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    gstbasesrc_class = (GstBaseSrcClass *) klass;
    gstpushsrc_class = (GstPushSrcClass *) klass;

    gobject_class->set_property = gst_prrtsrc_set_property;
    gobject_class->get_property = gst_prrtsrc_get_property;

    g_object_class_install_property (gobject_class, PROP_PORT,
        g_param_spec_uint ("port", "Port", 
            "The port to receive the packets from", 0, G_MAXUINT16,
            PRRT_DEFAULT_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (gobject_class, PROP_CAPS,
        g_param_spec_boxed ("caps", "Caps",
          "The caps of the source pad", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    // add pad template
    gst_element_class_add_static_pad_template (gstelement_class, &src_factory);

    gst_element_class_set_static_metadata (gstelement_class,
        "PRRT packet receiver", "Source/Network",
        "Receive data over the network via PRRT",
        "Hieu Nguyen <khachieunk@gmail.com>");

    gstelement_class->change_state = gst_prrtsrc_change_state;

    gstbasesrc_class->get_caps = gst_prrtsrc_getcaps;
    // TODO decide_allocation, unlock, unlock_stop

}

/* _init() function is used to initialize a specific instance of prrtsrc type */
static void gst_prrtsrc_init (GstPRRTSrc *prrt_src) {
    GST_DEBUG ("gst_prrtsrc_init");

    // TODO set some default values for plugin
    prrt_src->port = PRRT_DEFAULT_PORT;

    gst_pad_set_query_function (GST_BASE_SRC_PAD (prrt_src), gst_prrtsrc_src_query);
    
    /* configure basesrc to be a live source */
    gst_base_src_set_live (GST_BASE_SRC (prrt_src), TRUE);
    /* make basesrc output a segment in time */
    gst_base_src_set_format (GST_BASE_SRC (prrt_src), GST_FORMAT_TIME);
    /* make basesrc set timestamps on outgoing buffers based on the running_time
    * when they were captured */
    gst_base_src_set_do_timestamp (GST_BASE_SRC (prrt_src), TRUE);
}

static void 
gst_prrtsrc_set_property (GObject * object, guint prop_id, const GValue * value, 
    GParamSpec * pspec) {
    GstPRRTSrc *prrtsrc = GST_PRRTSRC (object);

    switch (prop_id)
    {
    case PROP_PORT:
        prrtsrc->port = g_value_get_uint (value);
        GST_DEBUG ("port: %u", prrtsrc->port);
        break;
    case PROP_CAPS: {
        const GstCaps *new_caps_val = gst_value_get_caps (value);
        GstCaps *new_caps;
        GstCaps *old_caps;

        if (new_caps_val == NULL) {
            new_caps = gst_caps_new_any ();
        } else {
            new_caps = gst_caps_copy (new_caps_val);
        }

        GST_OBJECT_LOCK (prrtsrc);
        old_caps = prrtsrc->caps;
        prrtsrc->caps = new_caps;
        GST_OBJECT_UNLOCK (prrtsrc);

        if (old_caps)
            gst_caps_unref (old_caps);
        
        gst_pad_mark_reconfigure (GST_BASE_SRC_PAD (prrtsrc));
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void gst_prrtsrc_get_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec) {
    GstPRRTSrc *prrtsrc = GST_PRRTSRC (object);
    switch (prop_id)
    {
    case PROP_PORT:
        g_value_set_uint (value, prrtsrc->port);
        break;
    case PROP_CAPS:
        GST_OBJECT_LOCK (prrtsrc);
        gst_value_set_caps (value, prrtsrc->caps);
        GST_OBJECT_UNLOCK (prrtsrc);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
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

static GstCaps *gst_prrtsrc_getcaps (GstBaseSrc *src, GstCaps *filter) {
    GstPRRTSrc *prrtsrc;
    GstCaps *caps, *result;

    prrtsrc = GST_PRRTSRC (src);

    GST_OBJECT_LOCK (src);
    if ((caps == prrtsrc->caps))
        gst_caps_ref (caps);
    GST_OBJECT_UNLOCK (src);

    if (caps) {
        if (filter) {
            result = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
            gst_caps_unref (caps); 
        } else {
            result = caps;
        }
    } else {
        result = (filter) ? gst_caps_ref (filter) : gst_caps_new_any ();
    }

    return result;
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
