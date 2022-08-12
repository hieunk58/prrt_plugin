#include <gst/gst.h>

typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *video_mixer;
    GstElement *video_convert;
    GstElement *video_sink;
    GstElement *queue_video;

    // UDP
    GstElement *udpsrc;
    GstElement *h264parser_udp;
    GstElement *decodebin_udp;
    GstElement *timeoverlay_udp;
    GstElement *textoverlay_udp;
    GstElement *videocrop_udp;
    GstElement *queue_udp;

    // PRRT
    GstElement *prrtsrc;
    GstElement *h264parser_prrt;
    GstElement *decodebin_prrt;
    GstElement *timeoverlay_prrt;
    GstElement *textoverlay_prrt;
    GstElement *videocrop_prrt;
    GstElement *queue_prrt;

} CustomData;

static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data) {
    GMainLoop *loop = data;
    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
            g_print ("End-of-stream\n");
            g_main_loop_quit (loop);
            break;
        case GST_MESSAGE_ERROR: { 
            gchar *debug = NULL; 
            GError *err = NULL;

            gst_message_parse_error (msg, &err, &debug);

            g_print ("Error: %s\n", err->message); 
            g_error_free (err);

            if (debug) {
                g_print ("Debug details: %s\n", debug); g_free (debug);
            }

            g_main_loop_quit (loop);
            break; 
        }
        default:
            break;
    }
    return TRUE;
}

static void
new_src_pad_cb (GstElement *element, GstPad *src_pad, GstPad *sink_pad)
{
    gchar *name;
    name = gst_pad_get_name (src_pad);
    g_print ("A new pad %s is created\n", name);

    if (!strcmp(name, "src_0")) {
        if (gst_pad_link (src_pad, sink_pad) != GST_PAD_LINK_OK) {
            g_printerr ("Pad could not be linked\n");
        } else {
            g_print("Pad %s is linked\n", name);
        }
    }
    g_free (name);
}

static void
new_pad_cb (GstElement *element, GstPad *src_pad, GstPad *sink_pad)
{
    gchar *name;
    name = gst_pad_get_name (src_pad);
    g_print ("A new pad %s is created\n", name);

    if (!strcmp(name, "video_0")) {
        if (gst_pad_link (src_pad, sink_pad) != GST_PAD_LINK_OK) {
            g_printerr ("Pad could not be linked\n");
        } else {
            g_print("Pad %s is linked\n", name);
        }
    }
    g_free (name);
}

int main(int argc, char *argv[]) {
    if(!(argc == 3)) {
        printf("Receiver: Wrong number of arguments. Usage: port_prrt port_udp\n");
        return -1;
    }

    gushort port_prrt = (gushort) atoi(argv[1]);
    gushort port_udp = (gushort) atoi(argv[2]);

    GMainLoop *loop;
    GstBus *bus;
    guint watch_id;
    GstStateChangeReturn ret;

    CustomData data;

    // initialization
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    // make elements
    data.pipeline = gst_pipeline_new ("receiver_pipeline");

    data.video_mixer = gst_element_factory_make ("video_mixer", "video_mixer");
    data.video_convert = gst_element_factory_make ("video_convert", "video_convert");
    data.video_sink = gst_element_factory_make ("video_sink", "video_sink");
    data.queue_video = gst_element_factory_make ("queue", "queue_video");

    // udp
    data.udpsrc = gst_element_factory_make ("udpsrc", "udpsrc");
    data.h264parser_udp = gst_element_factory_make ("h264parser", "h264parser_udp");
    data.decodebin_udp = gst_element_factory_make ("decodebin", "decodebin_udp");
    data.timeoverlay_udp = gst_element_factory_make ("timeoverlay", "timeoverlay_udp");
    data.textoverlay_udp = gst_element_factory_make ("textoverlay", "textoverlay_udp");
    data.videocrop_udp = gst_element_factory_make ("videocrop", "videocrop_udp");
    data.queue_udp = gst_element_factory_make ("queue", "queue_udp");
    
    // prrt
    data.prrtsrc = gst_element_factory_make ("prrtsrc", "prrtsrc");
    data.h264parser_prrt = gst_element_factory_make ("h264parser", "h264parser_prrt");
    data.decodebin_prrt = gst_element_factory_make ("decodebin", "decodebin_prrt");
    data.timeoverlay_prrt = gst_element_factory_make ("timeoverlay", "timeoverlay_prrt");
    data.textoverlay_prrt = gst_element_factory_make ("textoverlay", "textoverlay_prrt");
    data.videocrop_prrt = gst_element_factory_make ("videocrop", "videocrop_prrt");
    data.queue_prrt = gst_element_factory_make ("queue", "queue_prrt");

    if (!data.video_mixer ||
        !data.video_convert ||
        !data.video_sink ||
        !data.queue_video ||
        !data.udpsrc ||
        !data.h264parser_udp ||
        !data.decodebin_udp ||
        !data.timeoverlay_udp ||
        !data.textoverlay_udp ||
        !data.videocrop_udp ||
        !data.queue_udp ||
        !data.prrtsrc ||
        !data.h264parser_prrt ||
        !data.decodebin_prrt ||
        !data.timeoverlay_prrt ||
        !data.textoverlay_prrt ||
        !data.videocrop_prrt ||
        !data.queue_prrt) {
        
        g_printerr ("Not all elements could be created.\n");
        return -1;
    } else if (!data.prrtsrc) {
        g_printerr ("Plugin prrtsrc not found.\n");
        return -1;
    }

    // set properties
    g_object_set (G_OBJECT (data.video_sink), "sync", 0, NULL);
    g_object_set (G_OBJECT (data.udpsrc), "port", port_udp, "reuse", 1, NULL);
    g_object_set (G_OBJECT (data.prrtsrc), "port", port_prrt, NULL);

    g_object_set (G_OBJECT (data.timeoverlay_udp), "valignment", 2, "halignment", 2, NULL);
    g_object_set (G_OBJECT (data.textoverlay_udp), "text", "UDP", "valignment", 
        2, "halignment", 0, "font-desc", "12", NULL);
    g_object_set (G_OBJECT (data.videocrop_udp), "right", 512, NULL);

    g_object_set (G_OBJECT (data.timeoverlay_prrt), "valignment", 2, "halignment", 2, NULL);
    g_object_set (G_OBJECT (data.textoverlay_prrt), "text", "PRRT", "valignment", 
        2, "halignment", 2, "font-desc", "12", NULL);
    g_object_set (G_OBJECT (data.videocrop_prrt), "left", 512, NULL);

    return 0;
}