// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gstphotocamerasrc.h"
#include "gstphotostreamsrc.h"

GST_DEBUG_CATEGORY(gst_photoplugin_debug);

static gboolean plugin_init(GstPlugin * plugin)
{
    gboolean ok = TRUE;

    GST_DEBUG_CATEGORY_INIT(gst_photoplugin_debug, "photoplugin", 0,
                            "Multi-backend camera source");

    gst_meta_register_custom_simple("photo-buffer");

    ok &= gst_element_register(plugin, "photocamerasrc", GST_RANK_PRIMARY + 1,
                               GST_TYPE_PHOTO_CAMERA_SRC);
    ok &= gst_element_register(plugin, "photostreamsrc", GST_RANK_PRIMARY + 1,
                               GST_TYPE_PHOTO_STREAM_SRC);

    return ok;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR,
                  photoplugin, "Multi-backend camera API plugin",
                  plugin_init, VERSION, "LGPL", PACKAGE, "https://sailfishos.org")
