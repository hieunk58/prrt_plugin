#!/usr/bin/env bash

targetdelay=5000
codingconf="1,1,[]"

sudo echo "" >> /dev/null

exec env GST_DEBUG=prrtsink:6,queue_dataflow:6 GST_DEBUG_DUMP_DOT_DIR=~/gst-debug-send-dot/ gst-launch-1.0 \
    filesrc location=$3 \
    ! qtdemux name=qt qt.video_0 \
    ! h264parse \
    ! mpegtsmux \
    ! rtpmp2tpay \
    ! queue name="prrtqueue" \
    ! prrtsink host=$1 port=$2 targetdelay=$targetdelay codingconfig=$codingconf
