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
