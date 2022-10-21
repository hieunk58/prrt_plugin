/*
 * GStreamer
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_PRRTSRC_H__
#define __GST_PRRTSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <stdint.h>
#include <string.h>

#include "../../PRRT/prrt/proto/socket.h"

G_BEGIN_DECLS

#define GST_TYPE_PRRTSRC \
  (gst_prrt_source_get_type())
#define GST_PRRTSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PRRTSRC,GstPrrtSource))
#define GST_PRRTSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PRRTSRC,GstPrrtSourceClass))
#define GST_IS_PRRTSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PRRTSRC))
#define GST_IS_PRRTSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PRRTSRC))

typedef struct _GstPrrtSource      GstPrrtSource;
typedef struct _GstPrrtSourceClass GstPrrtSourceClass;

struct _GstPrrtSource
{
  GstElement element;
  GstBaseSrc base_src;
  GstPushSrc push_src;

  GstCaps *caps;

  PrrtSocket *recv_socket;
  char* data;
  guint total_size;

  guint16 port;
  guint32 window;

  guint64 bytes_received;
  guint64 packets_received;

  gboolean caps_received;
};

struct _GstPrrtSourceClass
{
  GstElementClass parent_class;
  GstBaseSrcClass gstbasesrc_class;
  GstPushSrcClass gstpushsrc_class;

  GstStructure* (*get_stats)    (GstPrrtSource *source);
  void          (*new_stats)    (GstElement *element, guint64 bytes_received, guint64 packets_received);
};

GType gst_prrt_source_get_type (void);

GstStructure *gst_prrt_source_get_stats (GstPrrtSource *source);

G_END_DECLS

#endif /* __GST_PRRTSRC_H__ */
