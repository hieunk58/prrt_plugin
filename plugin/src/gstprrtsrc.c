#include "gstprrtsrc.h"

#include <gst/gst.h>
#include <unistd.h>

#define PRRT_DEFAULT_PORT 5000
#define PRRT_DEFAULT_TARGET_DELAY 1000000
// Mostly IP header 20, UDP header 8, other headers, MTU 1500
// max for data: 1500 - 28 = 1472 so set to 1400
#define PRRT_DEFAULT_MAX_BUF_SIZE 1400
#define PRRT_HEADER_SIZE 7
#define PRRT_RING_BUFF_SIZE 10

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
static GstFlowReturn gst_prrtsrc_fill (GstPushSrc *psrc, GstBuffer *outbuf);
static void gst_prrtsrc_worker(void* input);

static gboolean gst_prrtsrc_open (GstPRRTSrc *src);
static gboolean gst_prrtsrc_close(GstPRRTSrc *src);

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

    gstpushsrc_class->fill = gst_prrtsrc_fill;

}

/* _init() function is used to initialize a specific instance of prrtsrc type */
static void gst_prrtsrc_init (GstPRRTSrc *prrt_src) {
    GST_DEBUG ("gst_prrtsrc_init");

    // TODO set some default values for plugin
    prrt_src->port = PRRT_DEFAULT_PORT;
    prrt_src->max_buff_size = PRRT_DEFAULT_MAX_BUF_SIZE;
    prrt_src->cap_received = FALSE;

    // allocate memory for ring buffer
    prrt_src->ring_buff_size = PRRT_RING_BUFF_SIZE;
    prrt_src->ring_buffer = calloc (prrt_src->ring_buff_size, sizeof(guint8*));
    
    for (int i = 0; i < prrt_src->ring_buff_size; ++i) {
        prrt_src->ring_buffer[i] = calloc (1, sizeof(guint8));
    }

    prrt_src->ring_buff_frame_size = calloc (prrt_src->ring_buff_size, sizeof(guint8));

    // temp packet
    prrt_src->tmp_packet = calloc (prrt_src->max_buff_size, sizeof(guint8));

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
        if (!gst_prrtsrc_open (src))
            goto open_failed;
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
        gst_prrtsrc_close (src);
        break;
    default:
        break;
    }

    return ret;

open_failed:
    {
        GST_DEBUG_OBJECT (src, "failed to open socket");
        return GST_STATE_CHANGE_FAILURE;
    }
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

/*----HELPER FUNCTIONS-----*/
guint8 get_frame_number(guint8* arr) {
    return arr[0];
}

guint8 get_seq_number(guint8* arr) {
    guint8 res = arr[1];
    return (res << 8) + arr[2];
}

guint32 get_frame_size(guint8* arr) {
    guint32 res = (arr[3] << 24) + (arr[4] << 16) + (arr[5] << 8) + arr[6];
    return res;
}
/*----HELPER FUNCTIONS-----*/

// mutex controls ring buffer
GMutex lck[PRRT_RING_BUFF_SIZE];
GMutex lck_next;
int next = 0;
bool finished = false;

static void gst_prrtsrc_worker(void* input) {
    GstPushSrc *psrc = (GstPushSrc*) input;
    GstPRRTSrc *src = GST_PRRTSRC (psrc);

    struct sockaddr addr;

    int offset = 0, num_packets = 0;

    int32_t payload_size, data_size, frame_seq, original_frame_num, frame_num, frame_size;
    
    payload_size = PrrtSocket_receive_ordered_wait (src->used_socket, 
                        src->tmp_packet, &addr, src->max_buff_size);
    data_size = payload_size - PRRT_HEADER_SIZE;

    // Extract info from received packet
    frame_seq = get_seq_number (src->tmp_packet);
    original_frame_num = get_frame_number (src->tmp_packet) % src->ring_buff_size;
    frame_num = original_frame_num;
    frame_size = get_frame_size (src->tmp_packet);

    guint32 max_buff_data_size = src->max_buff_size - PRRT_HEADER_SIZE;
    while (true)
    {
        num_packets = frame_size / (max_buff_data_size) + 
                        (frame_size % max_buff_data_size != 0);
        
        g_mutex_lock (&lck[frame_num]);

        if (frame_size != src->ring_buff_frame_size[frame_num]) {
            src->ring_buffer[frame_num] = realloc (src->ring_buffer[frame_num],
                                                    frame_size);
            src->ring_buff_frame_size[frame_num] = frame_size;                             
        }

        while (frame_num == original_frame_num) {
            offset = frame_seq * (PRRT_DEFAULT_MAX_BUF_SIZE - PRRT_HEADER_SIZE);
            memcpy (src->ring_buffer[frame_num] + offset, 
                src->tmp_packet + PRRT_HEADER_SIZE, data_size);
            
            // get new packet
            payload_size = PrrtSocket_receive_ordered_wait (src->used_socket, 
                        src->tmp_packet, &addr, src->max_buff_size);
            data_size = payload_size - PRRT_HEADER_SIZE;
            frame_seq = get_seq_number (src->tmp_packet);
            frame_num = get_frame_number (src->tmp_packet) % src->ring_buff_size;
            frame_size = get_frame_size (src->tmp_packet);

            --num_packets;
        }
        g_mutex_unlock (&lck[original_frame_num]);

        g_mutex_lock (&lck_next);
        if (num_packets == 0) {
            next = original_frame_num;
            finished = true;
        }
        g_mutex_unlock(&lck_next);

        original_frame_num = get_frame_number (src->tmp_packet) % src->ring_buff_size;
    }                
}

// get cap
static void gst_prrtsrc_get_caps (GstPRRTSrc *src) {
    src->tmp_caps = malloc (PRRT_DEFAULT_MAX_BUF_SIZE * sizeof(guint8));
    struct sockaddr addr;
    src->cap_size = PrrtSocket_recv (src->used_socket, src->tmp_caps, &addr);

    if (src->cap_size < 0) {
        GST_ERROR_OBJECT (src, "Could not get cap from socket");
    }

    src->tmp_caps = realloc (src->tmp_caps, src->cap_size);
}

// set cap
static void gst_prrtsrc_set_caps (GstPRRTSrc *src) {
    if (src->cap_size > 0) {
        src->cap_received = TRUE;
        GstCaps *negotiated_caps = gst_caps_from_string (src->tmp_caps);
        src->caps = negotiated_caps;

        gst_pad_mark_reconfigure (GST_BASE_SRC_CAP (src));
        GST_DEBUG_OBJECT(src, "Set caps: %s", src->tmp_caps);
    }
    free (src->tmp_caps);
}

// write data received from prrt to buffer
static GstFlowReturn 
gst_prrtsrc_fill (GstPushSrc *psrc, GstBuffer *outbuf) {
    GstPRRTSrc *src = GST_PRRTSRC (psrc);

    if (!src->cap_received) {
        gst_prrtsrc_get_caps (src);
        gst_prrtsrc_set_caps (src);

        pthread_t t_worker;
        pthread_create (&t_worker, NULL, &gst_prrtsrc_worker, src);
    }

    while (true)
    {
        g_mutex_lock(&lck_next);
        if (finished) {
            finished = false;
            src->current_buffer = next;
            break;
        }
        g_mutex_unlock(&lck_next);
    }
    
    GstMemory *mem = gst_allocator_alloc (NULL, 
                        src->ring_buff_frame_size[src->current_buffer] - 4096, 
                        NULL);
    gst_buffer_append_memory (outbuf, mem)                    
    
    GstMapInfo map;
    gst_buffer_map (outbuf, &map, GST_MAP_WRITE);

    g_mutex_lock (&lck[src->current_buffer]);
    memcpy (map.data, src->ring_buffer[src->current_buffer],
            src->ring_buff_frame_size[src->current_buffer]);
    g_mutex_unlock (&lck[src->current_buffer]);

    gst_buffer_unmap (outbuf, &map);

    return GST_FLOW_OK;
}

static gboolean gst_prrtsrc_open (GstPRRTSrc *src) {
    GST_DEBUG ("gst_prrtsrc_open");
    src->used_socket = PrrtSocket_create (PRRT_DEFAULT_PORT, 
        PRRT_DEFAULT_TARGET_DELAY);
    GST_DEBUG ("Binding to port: %u", src->port);

    int res = PrrtSocket_bind (src->used_socket, "0.0.0.0", src->port);
    if (res != 0) {
        GST_ERROR_OBJECT(src, "Socket binding failed: %d.\n", res);
        return FALSE;
    }

    if (src->used_socket == NULL) {
        GST_ERROR ("could not create socket");
        return FALSE;
    }

    return TRUE;
}

static gboolean gst_prrtsrc_close(GstPRRTSrc *src) {
    GST_DEBUG ("gst_prrtsrc_close");
    PrrtSocket_close(src->used_socket);
    free(src->used_socket);
    
    return TRUE;
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
