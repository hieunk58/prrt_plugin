#ifndef __GST_PRRTSRC_H__
#define __GST_PRRTSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <stdint.h>
#include <string.h>

G_BEGIN_DECLS

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