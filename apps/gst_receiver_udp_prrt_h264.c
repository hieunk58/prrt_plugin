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
    GstPadTemplate *video_mixer_sink_pad_template;
    GstPad *video_mixer_udp_pad, *video_mixer_prrt_pad;
    GstPad *queue_udp_pad, *queue_prrt_pad;
    GstPad *h264parser_udp_pad, *h264parser_prrt_pad;
    GstPad *timeoverlay_udp_pad, *timeoverlay_prrt_pad;

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

    // Build the pipeline
    gst_bin_add_many (GST_BIN (data.pipeline),
        data.video_mixer,
        data.video_convert,
        data.video_sink,
        data.queue_video,
        data.udpsrc,
        data.h264parser_udp,
        data.decodebin_udp,
        data.timeoverlay_udp,
        data.textoverlay_udp,
        data.videocrop_udp,
        data.queue_udp,
        data.prrtsrc,
        data.h264parser_prrt,
        data.decodebin_prrt,
        data.timeoverlay_prrt,
        data.textoverlay_prrt,
        data.videocrop_prrt,
        data.queue_prrt, NULL);

    // link elements with always pads
    if (!gst_element_link_many (data.video_mixer, data.queue_video, 
                                data.video_convert, data.video_sink, NULL) ||
        !gst_element_link_many(data.h264parser_prrt, data.decodebin_prrt, NULL) ||
        !gst_element_link_many(data.timeoverlay_prrt, data.textoverlay_prrt, 
                                data.videocrop_prrt, NULL) ||
        !gst_element_link_many(data.h264parser_udp, data.decodebin_udp, NULL) ||
        !gst_element_link_many(data.timeoverlay_udp, data.textoverlay_udp, 
                                data.videocrop_udp, NULL) ||
        !gst_element_link_many(data.videocrop_prrt, data.queue_prrt, NULL) ||
        !gst_element_link_many(data.videocrop_udp, data.queue_udp, NULL)) 
    {
        g_printerr ("Link elements failed!!\n");
        return -1;
    }

    // link videomixer
    video_mixer_sink_pad_template = gst_element_class_get_pad_template (
        GST_ELEMENT_GET_CLASS (data.video_mixer), "sink_%u");
    // for UDP
    video_mixer_udp_pad = gst_element_request_pad (data.video_mixer, 
        video_mixer_sink_pad_template, NULL, NULL);
    g_print ("Request pad %s for UDP\n", gst_pad_get_name (video_mixer_udp_pad));
    queue_udp_pad = gst_element_get_static_pad (data.queue_udp, "src");
    // for PRRT
    video_mixer_prrt_pad = gst_element_request_pad (data.video_mixer, 
        video_mixer_sink_pad_template, NULL, NULL);
    g_print ("Request pad %s for PRRT\n", gst_pad_get_name (video_mixer_prrt_pad));
    queue_prrt_pad = gst_element_get_static_pad (data.queue_prrt, "src");

    if (gst_pad_link (queue_udp_pad, video_mixer_udp_pad) != GST_PAD_LINK_OK ||
        gst_pad_link (queue_prrt_pad, video_mixer_prrt_pad) != GST_PAD_LINK_OK) {
        g_printerr ("Could not link videomixer.\n");
        gst_object_unref (data.pipeline);
        return -1;
    }
    gst_object_unref (queue_udp_pad);
    gst_object_unref (queue_prrt_pad);

    // link h264 parser pad
    h264parser_prrt_pad = gst_element_get_static_pad (data.h264parser_prrt, "sink");
    g_signal_connect (data.prrtsrc, "pad-added", G_CALLBACK (new_pad_cb), 
        h264parser_prrt_pad);
    
    h264parser_udp_pad = gst_element_get_static_pad (data.h264parser_udp, "sink");
    g_signal_connect (data.udpsrc, "pad-added", G_CALLBACK (new_pad_cb), 
        h264parser_udp_pad);

    // link timeoverlay pad
    timeoverlay_prrt_pad = gst_element_get_static_pad (data.timeoverlay_prrt, "video_sink");
    g_signal_connect (data.decodebin_prrt, "pad-added", G_CALLBACK (new_src_pad_cb),
        timeoverlay_prrt_pad);
    timeoverlay_udp_pad = gst_element_get_static_pad (data.timeoverlay_udp, "video_sink");
    g_signal_connect (data.decodebin_udp, "pad-added", G_CALLBACK (new_src_pad_cb),
        timeoverlay_udp_pad);

    // Set second video position
    GstPad* sink_1 = gst_element_get_static_pad(data.video_mixer, "sink_1");
    g_object_set(sink_1, "xpos", 512, NULL);

    return 0;
}