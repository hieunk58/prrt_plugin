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
    GstElement *decoder_udp;
    GstElement *queue_udp;

    // PRRT
    GstElement *prrtsrc;
    GstElement *h264parser_prrt;
    GstElement *decoder_prrt;
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
