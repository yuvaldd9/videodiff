# **GStreamer Video Diff Plugin**

This project features a **`video_diff`** plugin that compares two video streams pixel-by-pixel and shows the differences.

## **Example Pipeline**

```bash
gst-launch-1.0 \
  videotestsrc pattern=ball is-live=true ! video/x-raw,format=RGB,width=320,height=240 ! queue ! video_diff name=diff \
  videotestsrc pattern=ball is-live=true ! video/x-raw,format=RGB,width=320,height=240 ! queue ! diff.sink_1 \
  diff.src ! videoconvert ! autovideosink
```

### **What Happens?**
- This pipeline uses two identical **ball patterns** from **`videotestsrc`**.
- The **`video_diff`** plugin compares the two streams and calculates the pixel difference.
- Since both patterns are identical, the result will be a **black screen** (because there’s no difference between them).

### **Why a Black Screen?**
Both video streams are identical, so the difference between them is zero. The **`video_diff`** plugin outputs a black screen where all pixel differences are zero.

### **How to See the Difference?**
To see the difference, use two different patterns. For example:

```bash
gst-launch-1.0 \
  videotestsrc pattern=ball is-live=true ! video/x-raw,format=RGB,width=320,height=240 ! queue ! video_diff name=diff \
  videotestsrc pattern=smpte is-live=true ! video/x-raw,format=RGB,width=320,height=240 ! queue ! diff.sink_1 \
  diff.src ! videoconvert ! autovideosink
```

This will show the difference between a **ball** and a **smpte** pattern.
