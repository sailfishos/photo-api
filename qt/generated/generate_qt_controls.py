# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

import os
import sys
import yaml

with open(sys.argv[1], 'r') as file:
    control_list = yaml.load(file.read(), yaml.SafeLoader)

header = open(os.path.join(sys.argv[2], 'ctl-classes.h'), 'w')
enum_reg = open(os.path.join(sys.argv[2], 'ctl-enum-registration.h'), 'w')

header.write('''#ifndef PHOTO_API_QT_GENERATED_CLASSES_H
#define PHOTO_API_QT_GENERATED_CLASSES_H

#include "controls.h"

namespace photo_api_qt {
''')

for control in control_list:
    if control.get('readonly'):
        continue

    if 'values' in control:
        symbol = ''.join([s[0] + s[1:].lower() for s in control['id'].split('_')])

        enum_reg.write(f'qmlRegisterUncreatableType<{symbol}Control>(uri, 0, 1, "{symbol}", "control is part of camera");\n')
        header.write(f'''
class {symbol}Control : public EnumControl {"{"}
    Q_OBJECT
    Q_PROPERTY(Enum value READ value WRITE setValue NOTIFY valueChanged)

public:
    enum Enum {"{"}
''')
        for value in control['values']:
            enum_symbol = ''.join([s[0] + s[1:].lower() for s in value['id'].split('_')])
            header.write(f'        {enum_symbol},\n')
        header.write(f'''    {"}"};
    Q_ENUM(Enum)

    explicit {symbol}Control(Camera *camera, PhotoControlId id)
        : EnumControl(camera, id, {len(control["values"])})
    {"{}"}

    Enum value() const {"{"} return (Enum)m_value; {"}"}

signals:
    void valueChanged();
{"}"};
''')

constructor_initializers = []

header.write('''
class ControlContainer : public QObject
{
    Q_OBJECT
''')

def create_property(t, name, ctl_id):
    header.write(f'    {t} m_{name};\n')
    header.write(f'    {t} *{name}() {"{"} return &m_{name}; {"}"}\n')
    header.write(f'    Q_PROPERTY(photo_api_qt::{t} *{name} READ {name} CONSTANT)\n')
    constructor_initializers.append(f'm_{name}(camera, PHOTO_CONTROL_{ctl_id})')

for control in control_list:
    if control.get('readonly'):
        continue

    symbol = ''.join([s[0] + s[1:].lower() for s in control['id'].split('_')])
    symbol_l = symbol[0].lower() + symbol[1:]
    if 'values' in control:
        type_name = 'enum'
    else:
        type_name = control['type']

    if type_name == 'areas':
        create_property('MeteringAreas', symbol_l, control['id'])
    elif type_name == 'enum':
        create_property(f'{symbol}Control', symbol_l, control['id'])
    else:
        create_property('FloatRangeControl', symbol_l, control['id'])

constructor_initializers = '\n        , '.join(constructor_initializers)
header.write(f'''
public:
    ControlContainer(Camera *camera)
        : {constructor_initializers}
    {"{}"}
{"}"};

{"}"} /* namespace */

#endif
''')
