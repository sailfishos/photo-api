# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

core_sources = [
  'photo-api.c',
  'photo-controls.c',
  'photo-egl.c',
]

core_headers = [
  'photo-api.h',
  'photo-api-types.h',
  'photo-backend.h',
  'photo-controls.h',
  'photo-control-types.h',
  'photo-egl.h',
]

header_subdir = 'photo-api-@0@'.format(api_version)

install_headers(core_headers, subdir: header_subdir)

subdir('generated')

core_library = library(
  'photo-api-@0@'.format(api_version),
  core_sources + core_headers + core_generated[0],
  dependencies: [egl_dep, glib_dep],
  include_directories: [core_inc],
  version: lib_version,
  soversion: so_version,
  install: true
)

core_dep = declare_dependency(
  link_with: core_library,
  sources: core_generated[0],
  include_directories: [core_inc]
)

pkgconfig.generate(
  core_library,
  subdirs: header_subdir,
  variables: ['plugin_dir=${libdir}/photo-plugins'],
)
