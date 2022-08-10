#include <gst/gst.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

gint main(gint argc, gchar *argv[]) {
    if (argc != 5) {
        g_print ("Wrong argument. Usage: host prrt_port udp_port file_location\n");
        return 01;
    }

    // pass argument: host, port, file location
    gchar* host = argv[1];
    gushort prrt_port = (gushort) atoi (argv[2]);
    gushort udp_port = (gushort) atoi (argv[3]);
    gchar* file_location = argv[4];

    GstStateChangeReturn ret;
    GstElement *pipeline, *filesrc, *tee;
    GstElement *queue_prrt, *prrtsink;
    GstElement *queue_udp, *udpsink;
    GMainLoop *loop;
    GstBus *bus;
    guint watch_id;

    GstPadTemplate *tee_src_pad_template;
    GstPad *tee_udp_pad, *tee_prrt_pad;
    GstPad *queue_udp_pad, *queue_prrt_pad;

    GstPad *demux_src_pad, *udpsink_pad;

    // initialization
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    // create elements
    pipeline = gst_pipeline_new ("sender_pipeline");

    filesrc = gst_element_factory_make ("filesrc", "sender_filesource");
    tee = gst_element_factory_make ("tee", "tee");

    // prrt part
    queue_prrt = gst_element_factory_make ("queue", "queue_prrt");
    prrtsink   = gst_element_factory_make ("prrtsink", "prrtsink");
    // udp part
    queue_udp = gst_element_factory_make ("queue", "queue_udp");
    udpsink   = gst_element_factory_make ("udpsink", "udpsink");

    if (!filesrc || !tee || !queue_prrt || !queue_udp || !udpsink) {
        g_print ("Elements not found - check your install\n");
        return -1;
    } else if (!prrtsink) {
        g_print ("Prrt sink plugin not found\n");
        return -1;
    }

    g_object_set (G_OBJECT (filesrc), "file_location", file_location, NULL);
    g_object_set (G_OBJECT (prrtsink), "host", host, "port", prrt_port, NULL);
    g_object_set (G_OBJECT (udpsink), "host", host, "port", udp_port, NULL);

    gst_bin_add_many (GST_BIN (pipeline), filesrc, tee, queue_prrt, queue_udp,
        udpsink, prrtsink, NULL);

    // link elements with always pad together
    if (!gst_element_link_many (filesrc, tee, NULL) ||
        !gst_element_link_many (queue_prrt, prrtsink, NULL) ||
        !gst_element_link_many (queue_udp, udpsink, NULL)) {
        g_print ("Failed to link one or more elements!\n");
        return -1;
    }

    // link the tee which has request pads
    tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (tee), 
                                                                "src_%u");
    tee_udp_pad = gst_element_request_pad (tee, tee_src_pad_template, NULL, NULL);
    g_print ("Obtained request pad %s for udp branch.\n", gst_pad_get_name (tee_udp_pad));
    queue_udp_pad = gst_element_get_static_pad (queue_udp, "sink");

    tee_prrt_pad = gst_element_request_pad (tee, tee_src_pad_template, NULL, NULL);
    g_print ("Obtained request pad %s for prrt branch.\n", gst_pad_get_name (tee_prrt_pad));
    queue_prrt_pad = gst_element_get_static_pad (queue_prrt, "sink");

    if (gst_pad_link (tee_udp_pad, queue_udp_pad) != GST_PAD_LINK_OK ||
        gst_pad_linnk (tee_prrt_pad, queue_prrt_pad) != GST_PAD_LINK_OK) {
        g_print ("Could not link Tee!\n");
        return -1;
    }

    gst_object_unref (queue_udp_pad);
    gst_object_unref (queue_prrt_pad);

    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    // run
    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        GstMessage *msg;
        g_print ("Failed to start up pipeline!\n");
        /* check if there is an error message with details on the bus */
        msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
        if (msg) {
            GError *err = NULL;
            gst_message_parse_error (msg, &err, NULL);
            g_print ("ERROR: %s\n", err->message);
            g_error_free (err);
            gst_message_unref (msg);
        }
        return -1;
    }

    g_main_loop_run (loop);

    // clean up
    // release requests pads from tee
    gst_element_release_request_pad (tee, tee_udp_pad);
    gst_element_release_request_pad (tee, tee_prrt_pad);
    gst_object_unref (tee_udp_pad);
    gst_object_unref (tee_prrt_pad);
    
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    g_source_remove (watch_id);
    g_main_loop_unref (loop);

    return 0;
}
