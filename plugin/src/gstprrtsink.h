#ifndef __GST_PRRTSINK_H__
#define __GST_PRRTSINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "../../PRRT/prrt/proto/socket.h"

G_BEGIN_DECLS

#define GST_TYPE_PRRTSINK \
  (gst_prrt_sink_get_type())
#define GST_PRRTSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PRRTSINK,GstPRRTSink))
#define GST_PRRTSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PRRTSINK,GstPRRTSinkClass))
#define GST_IS_PRRTSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PRRTSINK))
#define GST_IS_PRRTSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PRRTSINK))

typedef struct _GstPRRTSink      GstPRRTSink;
typedef struct _GstPRRTSinkClass GstPRRTSinkClass;

struct _GstPRRTSink {
  GstBaseSink parent;

  // socket
  PrrtSocket *used_socket;

  // properties
  gchar* host;
  guint16 port;
  guint32 max_buff_size;
  guint32 target_delay;
  gboolean negotiate_cap;

  guint32 frame_size;

  guint8  header_frame;
  guint16 header_seq;
  guint32 header_size;
  
  guint32 packet_data_size;
  guint8* tmp_packet;
};

struct _GstPRRTSinkClass {
    GstBaseSinkClass parent_class;
};

GType gst_prrt_sink_get_type (void);

G_END_DECLS

#endif //  __GST_PRRTSINK_H__