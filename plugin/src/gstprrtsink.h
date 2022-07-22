#ifndef __GST_PRRTSINK_H__
#define __GST_PRRTSINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

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

    // properties
    gchar* host;
    guint16 port;

};

struct _GstPRRTSinkClass {
    GstBaseSinkClass parent_class;
};

GType gst_prrt_sink_get_type (void);

G_END_DECLS

#endif //  __GST_PRRTSINK_H__