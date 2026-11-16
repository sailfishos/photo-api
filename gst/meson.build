# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: LGPL-2.1-or-later

gst_plugin_sources = [
  'gstphotoallocator.c',
  'gstphotobufferpool.c',
  'gstphotocamerasrc.c',
  'gstphotoplugin.c',
  'gstphotosrcpad.c',
  'gstphotostreamsrc.c',
]

gst_plugin_deps = [
  core_dep,
  gst_dep,
  gstbase_dep,
  gstcamerabinsrc_dep,
  gstnemointerfaces_dep,
  gstvideo_dep,
]

shared_module(
  'gstphotoplugin',
  gst_plugin_sources,
  c_args: ['-DGST_USE_UNSTABLE_API',
           '-DPACKAGE="@0@"'.format(package_name),
           '-DVERSION="@0@"'.format(lib_version)],
  include_directories: [core_inc],
  dependencies: gst_plugin_deps,
  install: true,
  install_dir: join_paths(get_option('libdir'), 'gstreamer-1.0'),
)
