#include <gst/gst.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
