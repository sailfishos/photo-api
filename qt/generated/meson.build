# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

qt_generated = custom_target(
  'qt_generated',
  output: [
    'ctl-classes.h',
    'ctl-enum-registration.h'
  ],
  input: ['generate_qt_controls.py', '../../controls.yaml'],
  command: [python, '@INPUT0@', '@INPUT1@', '@OUTDIR@']
)

# This is a workaround for the meson qt module not supporting
# custom_target_index instances as moc inputs
qt_generated_classes_h = custom_target(
  'qt_generated_classes_h',
  input: [
    qt_generated[0]
  ],
  output: [
    'ctl-classes-meson_workaround.h'
  ],
  command: ['cp', '@INPUT@', '@OUTPUT@']
)
