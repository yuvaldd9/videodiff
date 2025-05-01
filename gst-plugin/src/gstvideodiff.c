#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include "gstvideodiff.h"

GST_DEBUG_CATEGORY_STATIC (gst_video_diff_debug);
#define GST_CAT_DEFAULT gst_video_diff_debug

enum
{
  PROP_0,
  PROP_SILENT
};

static GstStaticPadTemplate sink_factory_0 = GST_STATIC_PAD_TEMPLATE (
  "sink_0", GST_PAD_SINK, GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("video/x-raw, format=RGB")
);

static GstStaticPadTemplate sink_factory_1 = GST_STATIC_PAD_TEMPLATE (
  "sink_1", GST_PAD_SINK, GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("video/x-raw, format=RGB")
);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE (
  "src", GST_PAD_SRC, GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("video/x-raw, format=RGB")
);

#define gst_video_diff_parent_class parent_class
G_DEFINE_TYPE (GstVideoDiff, gst_video_diff, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE (
  video_diff, "video_diff", GST_RANK_NONE, GST_TYPE_VIDEODIFF
);

// Forward declarations
static GstFlowReturn try_process_and_push(GstVideoDiff *filter);
static GstFlowReturn gst_video_diff_chain_0 (GstPad * pad, GstObject * parent, GstBuffer * buf);
static GstFlowReturn gst_video_diff_chain_1 (GstPad * pad, GstObject * parent, GstBuffer * buf);
static gboolean gst_video_diff_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);

static void
gst_video_diff_set_property (GObject * object, guint prop_id,
                             const GValue * value, GParamSpec * pspec);
static void
gst_video_diff_get_property (GObject * object, guint prop_id,
                             GValue * value, GParamSpec * pspec);

static void
gst_video_diff_class_init (GstVideoDiffClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_video_diff_set_property;
  gobject_class->get_property = gst_video_diff_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output?",
                            FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple (gstelement_class,
      "VideoDiff", "Filter/Effect/Video",
      "Subtracts pixels between two video streams",
      "didi <<user@hostname.org>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory_0));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory_1));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
}

static void
gst_video_diff_init (GstVideoDiff * filter)
{
  filter->sinkpad_0 = gst_pad_new_from_static_template (&sink_factory_0, "sink_0");
  gst_pad_set_event_function (filter->sinkpad_0,
      GST_DEBUG_FUNCPTR (gst_video_diff_sink_event));
  gst_pad_set_chain_function (filter->sinkpad_0,
      GST_DEBUG_FUNCPTR (gst_video_diff_chain_0));
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad_0);

  filter->sinkpad_1 = gst_pad_new_from_static_template (&sink_factory_1, "sink_1");
  gst_pad_set_event_function (filter->sinkpad_1,
      GST_DEBUG_FUNCPTR (gst_video_diff_sink_event));
  gst_pad_set_chain_function (filter->sinkpad_1,
      GST_DEBUG_FUNCPTR (gst_video_diff_chain_1));
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad_1);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->silent = FALSE;
  filter->buffer_0 = NULL;
  filter->buffer_1 = NULL;
}

static void
gst_video_diff_set_property (GObject * object, guint prop_id,
                             const GValue * value, GParamSpec * pspec)
{
  GstVideoDiff *filter = GST_VIDEODIFF (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_diff_get_property (GObject * object, guint prop_id,
                             GValue * value, GParamSpec * pspec)
{
  GstVideoDiff *filter = GST_VIDEODIFF (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_video_diff_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstVideoDiff *filter = GST_VIDEODIFF (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
                  GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CAPS: {
      GstCaps *caps;
      gst_event_parse_caps(event, &caps);
      GST_DEBUG_OBJECT(filter, "Handling CAPS event on pad %s: %" GST_PTR_FORMAT,
                       GST_PAD_NAME(pad), caps);
      return gst_pad_push_event(filter->srcpad, gst_event_ref(event));
    }
    default:
      break;
  }

  return gst_pad_event_default(pad, parent, event);
}

static GstFlowReturn
gst_video_diff_chain_0 (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstVideoDiff *filter = GST_VIDEODIFF (parent);

  if (filter->buffer_0)
    gst_buffer_unref (filter->buffer_0);
  filter->buffer_0 = gst_buffer_ref (buf);

  return try_process_and_push (filter);
}

static GstFlowReturn
gst_video_diff_chain_1 (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstVideoDiff *filter = GST_VIDEODIFF (parent);

  if (filter->buffer_1)
    gst_buffer_unref (filter->buffer_1);
  filter->buffer_1 = gst_buffer_ref (buf);

  return try_process_and_push (filter);
}

static GstFlowReturn
try_process_and_push (GstVideoDiff * filter)
{
  if (!filter->buffer_0 || !filter->buffer_1)
    return GST_FLOW_OK;

  GstMapInfo map0, map1;
  if (!gst_buffer_map (filter->buffer_0, &map0, GST_MAP_READ)) return GST_FLOW_ERROR;
  if (!gst_buffer_map (filter->buffer_1, &map1, GST_MAP_READ)) {
    gst_buffer_unmap (filter->buffer_0, &map0);
    return GST_FLOW_ERROR;
  }

  if (map0.size != map1.size) {
    GST_ERROR_OBJECT (filter, "Buffers have different sizes!");
    gst_buffer_unmap (filter->buffer_0, &map0);
    gst_buffer_unmap (filter->buffer_1, &map1);
    return GST_FLOW_ERROR;
  }

  GstBuffer *out_buf = gst_buffer_new_allocate (NULL, map0.size, NULL);
  GstMapInfo out_map;
  gst_buffer_map (out_buf, &out_map, GST_MAP_WRITE);

  // Parallel per-channel absolute difference (RGB assumed)
  #pragma omp parallel for
  for (gsize i = 0; i < map0.size - 2; i += 3) {
      out_map.data[i]     = abs((int)map0.data[i]     - (int)map1.data[i]);     // R
      out_map.data[i + 1] = abs((int)map0.data[i + 1] - (int)map1.data[i + 1]); // G
      out_map.data[i + 2] = abs((int)map0.data[i + 2] - (int)map1.data[i + 2]); // B
  }

  gst_buffer_unmap (filter->buffer_0, &map0);
  gst_buffer_unmap (filter->buffer_1, &map1);
  gst_buffer_unmap (out_buf, &out_map);

  GST_BUFFER_PTS (out_buf) = GST_BUFFER_PTS (filter->buffer_0);
  GST_BUFFER_DTS (out_buf) = GST_BUFFER_DTS (filter->buffer_0);
  GST_BUFFER_DURATION (out_buf) = GST_BUFFER_DURATION (filter->buffer_0);

  gst_buffer_unref (filter->buffer_0);
  gst_buffer_unref (filter->buffer_1);
  filter->buffer_0 = NULL;
  filter->buffer_1 = NULL;

  return gst_pad_push (filter->srcpad, out_buf);
}

// Plugin initialization
static gboolean
videodiff_init (GstPlugin * videodiff)
{
  GST_DEBUG_CATEGORY_INIT (gst_video_diff_debug, "videodiff", 0, "Video diff plugin");
  return GST_ELEMENT_REGISTER (video_diff, videodiff);
}

#ifndef PACKAGE
#define PACKAGE "videodiff"
#endif

GST_PLUGIN_DEFINE (
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  videodiff,
  "Subtract pixels from two video streams",
  videodiff_init,
  PACKAGE_VERSION,
  GST_LICENSE,
  GST_PACKAGE_NAME,
  GST_PACKAGE_ORIGIN
)
