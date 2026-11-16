# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

core_generated = custom_target(
  'core_generated',
  output: [
    'ctl-data-fields.h',
    'ctl-ids.h',
    # also generates 'ctl-list.h', but it is private and should not be installed
  ],
  input: ['generate_controls.py', '../../controls.yaml'],
  command: [python, '@INPUT0@', '@INPUT1@', '@OUTDIR@'],
  install: true,
  install_dir: join_paths(get_option('includedir'), header_subdir, 'generated')
)
