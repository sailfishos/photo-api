# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

project(
  'photo-api',
  'c', 'cpp',
  version: '0.1.0',
  default_options: [
    'warning_level=1',
    'werror=true',
    'buildtype=debugoptimized'
  ]
)

package_name = meson.project_name()
lib_version = meson.project_version()
api_version = '0.1'
so_version = '0'

cc = meson.get_compiler('c')

plugin_dir = join_paths(get_option('libdir'), 'photo-plugins')
add_project_arguments(
  [
    '-DPHOTO_API_PRIVATE',
    '-DPLUGIN_DIR="@0@"'.format(join_paths(get_option('prefix'), plugin_dir)),
  ],
  language: 'c'
)

glib_dep = dependency('glib-2.0', required: true)
egl_dep = dependency('egl', required: true)

# GStreamer integration
gst_dep = dependency('gstreamer-1.0', required: true)
gstbase_dep = dependency('gstreamer-base-1.0', required: true)
gstvideo_dep = dependency('gstreamer-video-1.0', required: true)
gstcamerabinsrc_dep = cc.find_library('gstbasecamerabinsrc-1.0', required: true)
gstnemointerfaces_dep = dependency('nemo-gstreamer-interfaces-1.0', required: true)

# Qt integration
qt5 = import('qt5')
qt_core_dep = dependency('Qt5Core', required: true)
qt_gui_dep = dependency('Qt5Gui', required: true)
qt_gui_private_inc = include_directories(
  join_paths(qt_gui_dep.get_variable('includedir'), 'QtGui', qt_gui_dep.version(), 'QtGui')
)
qt_quick_dep = dependency('Qt5Quick', required: true)

core_inc = include_directories('lib')

pkgconfig = import('pkgconfig')
python = find_program('python3')

subdir('lib')

subdir('gst')
subdir('qt')
subdir('tools')
