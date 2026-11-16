# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

import os
import sys
import yaml

with open(sys.argv[1], 'r') as file:
    control_list = yaml.load(file.read(), yaml.SafeLoader)

ctl_data_fields = open(os.path.join(sys.argv[2], 'ctl-data-fields.h'), 'w')
ctl_ids = open(os.path.join(sys.argv[2], 'ctl-ids.h'), 'w')
ctl_list = open(os.path.join(sys.argv[2], 'ctl-list.h'), 'w')

ctl_ids.write('typedef enum {\n')
ctl_list.write('''static const struct {
    const char *name;
    PhotoControlType type;
    bool read_only;
    unsigned int array_size;
    unsigned int info_offset;
    unsigned int value_offset;
    unsigned int enum_count;
    const char * const *enum_value_names;
} photo_controls[PHOTO_CONTROL_ID_NUM] = {
''')

for control in control_list:
    readonly = control.get('readonly', False)
    symbol = control['id']
    symbol_l = symbol.lower()
    if 'values' in control:
        type_name = 'enum'
        num_values = len(control['values'])
    else:
        type_name = control['type']
        num_values = 0

    info = None
    data = None

    if type_name == 'areas':
        data = f'PhotoControlMeteringAreas {symbol_l}'
    elif type_name == 'enum':
        info = f'PhotoBitMask {symbol_l}_info[PHOTO_BITMASK_SIZE({num_values})]'
        data = f'unsigned int {symbol_l}'
    elif type_name == 'float':
        info = f'PhotoControlFloatRange {symbol_l}_info'
        data = f'float {symbol_l}'
    else:
        raise ValueError(f'Invalid "type": "{type_name}"')

    total_size = 1

    if 'array' in control:
        sizes = control['array']
        if type(sizes) is not list:
            raise TypeError(f'"array" must be a list, e.g. [M, N]')

        for size in sizes:
            total_size *= size

        data += f'[{total_size}]'

    if not readonly:
        if info is not None:
            ctl_data_fields.write(f'{info};\n')
        if data is not None:
            ctl_data_fields.write(f'{data};\n')

    ctl_ids.write(f'    PHOTO_CONTROL_{symbol},\n')

    ctl_list.write('    {\n')
    ctl_list.write(f'        .name = "{symbol}",\n')
    ctl_list.write(f'        .type = PHOTO_CONTROL_TYPE_{type_name.upper()},\n')
    ctl_list.write(f'        .read_only = {"true" if readonly else "false"},\n')
    ctl_list.write(f'        .array_size = {total_size},\n')
    if not readonly:
        if type_name != 'areas':
            ctl_list.write(f'        .info_offset = (unsigned char *) &((struct _PhotoCamera *) 0)->{symbol_l}_info - ((struct _PhotoCamera *) 0)->_controls,\n')
        ctl_list.write(f'        .value_offset = (unsigned char *) &((struct _PhotoCamera *) 0)->{symbol_l} - ((struct _PhotoCamera *) 0)->_controls,\n')

    if 'values' in control:
        ctl_list.write(f'        .enum_count = {num_values},\n')
        ctl_list.write('        .enum_value_names = (const char *[]){\n')
        for value in control['values']:
            ctl_list.write(f'            "{value["id"]}",\n')
        ctl_list.write('        },\n')

    ctl_list.write('    },\n')

ctl_ids.write('''    PHOTO_CONTROL_ID_NUM
} PhotoControlId;
''')
ctl_list.write('};\n')

for control in control_list:
    if 'values' in control:
        enum_name = ''.join([s[0] + s[1:].lower() for s in control['id'].split('_')])

        ctl_ids.write('typedef enum {\n')
        for value in control['values']:
            ctl_ids.write(f'    PHOTO_{value["id"]},\n')
        ctl_ids.write(f'{"}"} PhotoControl{enum_name};\n')
