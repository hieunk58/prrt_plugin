#!/usr/bin/env bash

window=41000

exec env GST_DEBUG=prrtsrc:6,queue_dataflow:6 GST_DEBUG_DUMP_DOT_DIR=~/gst-debug-recv-dot/ gst-launch-1.0 \
    prrtsrc port=$1 window=$window \
    ! queue name="prrtqueue" \
    ! 'application/x-rtp, media=video, clock-rate=90000, encoding-name=MP2T, payload=33' \
    ! rtpmp2tdepay \
    ! tsdemux name=d d.video_0_0041 \
    ! h264parse \
    ! avdec_h264 name=prrt_decoder \
    ! queue name="prrt2queue" \
    ! videoscale name=prrt_scale \
    ! 'video/x-raw, width=1024, height=576' \
    ! queue \
    ! videoconvert name=prrt_convert \
    ! ximagesink
