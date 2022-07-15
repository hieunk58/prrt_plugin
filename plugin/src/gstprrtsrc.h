#ifndef __GST_PRRTSRC_H__
#define __GST_PRRTSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <stdint.h>
#include <string.h>

G_BEGIN_DECLS

#define GST_TYPE_PRRTSRC (gst_prrtsrc_get_type())
#define GST_PRRTSRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PRRTSRC, GstPRRTSrc))
#define GST_PRRTSRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_PRRTSRC, GstControlSourceClass))
#define GST_IS_PRRTSRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PRRTSRC))
#define GST_IS_PRRTSRC_CLASS(klass) (G_TYPE_CHECK_INSTANCE_TYPE((klass), GST_TYPE_PRRTSRC))

typedef struct _GstPRRTSrc      GstPRRTSrc;
typedef struct _GstPRRTSrcClass GstPRRTSrcClass;

struct _GstPRRTSrc
{
    GstPushSrc parent;

    /* socket */
    //PrrtSocket *recv_socket;

    /* properties */
    gint port;
    guint32 max_buff_size;


    /* ring buffer */

};

struct _GstPRRTSrcClass
{
    GstPushSrcClass parent_class;
};

GType gst_prrtsrc_get_type (void);

G_END_DECLS


#endif // __GST_PRRTSRC_H__