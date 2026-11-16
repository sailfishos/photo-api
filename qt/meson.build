# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

qmlplugin_dir = join_paths(get_option('libdir'), 'qt5/qml/org/sailfishos/PhotoApi')

subdir('generated')

qt_api_sources = [
  'basecapturetarget.cpp',
  'camera.cpp',
  'cameramanager.cpp',
  'controls.cpp',
  'imagecapture.cpp',
  'qmlplugin.cpp',
  'streamtarget.cpp',
  'viewfinder.cpp',
]

qt_api_headers = [
  'basecapturetarget.h',
  'camera.h',
  'cameramanager.h',
  'controls.h',
  'imagecapture.h',
  'streamtarget.h',
  'viewfinder.h',
]

qt_api_deps = [
  core_dep,
  egl_dep,
  qt_core_dep,
  qt_quick_dep
]

qt_api_moc = qt5.preprocess(
  moc_sources: 'qmlplugin.cpp',
  moc_headers: qt_api_headers + qt_generated_classes_h,
  dependencies: qt_api_deps
)

shared_module(
  'qmlphotoapi',
  qt_api_sources + qt_api_headers + qt_generated, qt_api_moc,
  dependencies: qt_api_deps,
  include_directories: qt_gui_private_inc,
  install: true,
  install_dir: qmlplugin_dir
)

install_data(['qmldir'], install_dir: qmlplugin_dir)
