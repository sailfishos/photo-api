# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

project(
  'photo-api',
  'c',
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

core_inc = include_directories('lib')

pkgconfig = import('pkgconfig')
python = find_program('python3')

subdir('lib')
