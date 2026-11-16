"""Build a reproducible prototype rig and clips for the project's existing GLB."""
import json
import math
import struct
import sys
from pathlib import Path

source = Path(sys.argv[1]).read_bytes()
json_size = struct.unpack_from('<I', source, 12)[0]
original = json.loads(source[20:20 + json_size])
bin_header = 20 + json_size
bin_size, bin_kind = struct.unpack_from('<I4s', source, bin_header)
if source[:4] != b'glTF' or bin_kind != b'BIN\0':
    raise ValueError('expected a binary glTF with JSON and BIN chunks')
binary = bytearray(source[bin_header + 8:bin_header + 8 + bin_size])
views = original['bufferViews']
accessors = original['accessors']

names = ['Root', 'Hips', 'Spine', 'Chest', 'Neck', 'Head',
         'L_UpperArm', 'L_Forearm', 'L_Hand', 'R_UpperArm', 'R_Forearm', 'R_Hand',
         'L_Thigh', 'L_Calf', 'L_Foot', 'R_Thigh', 'R_Calf', 'R_Foot',
         'Cape_Root', 'Cape_Mid', 'Cape_Tip']
parents = [-1, 0, 1, 2, 3, 4, 3, 6, 7, 3, 9, 10, 1, 12, 13, 1, 15, 16, 3, 18, 19]
pivots = [(0,0,0),(0,.88,0),(0,1,0),(0,1.42,0),(0,1.68,0),(0,1.86,0),
          (-.32,1.58,0),(-.43,1.20,0),(-.48,.96,0),(.32,1.58,0),(.43,1.20,0),(.48,.96,0),
          (-.18,.62,0),(-.18,.39,0),(-.18,.13,.02),(.18,.62,0),(.18,.39,0),(.18,.13,.02),
          (0,1.62,-.22),(0,.96,-.28),(0,.28,-.36)]
primary = [2,3,3,1,1,1,5,5,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,1,1,1]
secondary = [1,2,2,2,1,2,4,4,4,7,8,7,10,11,10,13,14,13,16,17,16,19,20,19,2,2,2]


def add(values, fmt, kind, component, count):
    while len(binary) % 4:
        binary.append(0)
    start = len(binary)
    binary.extend(struct.pack('<' + fmt * len(values), *values))
    views.append({'buffer': 0, 'byteOffset': start, 'byteLength': len(binary) - start})
    accessors.append({'bufferView': len(views)-1, 'componentType': component,
                      'type': kind, 'count': count})
    return len(accessors)-1


def smooth(x):
    x = min(1.0, max(0.0, x))
    return x*x*(3-2*x)


def blend(part, y):
    if part in (9,12): return 1-smooth((y-1.18)/.28)
    if part in (10,13): return 1-smooth((y-.94)/.22)
    if part in (11,14): return .18
    if part in (15,18): return 1-smooth((y-.42)/.30)
    if part in (16,19): return 1-smooth((y-.16)/.18)
    if part in (17,20): return .15
    if part == 21: return .22
    if part == 22: return 1-smooth((y-.72)/.58)
    if part == 23: return .25+.35*smooth((.7-y)/.6)
    if part == 0: return .18
    if part in (1,2,5): return .12
    return .04


primitives = []
if len(original['meshes']) != len(primary):
    raise ValueError('unexpected source character mesh layout')
for part, mesh in enumerate(original['meshes']):
    if len(mesh['primitives']) != 1:
        raise ValueError('expected one primitive per source part')
    prim = mesh['primitives'][0]
    pos = accessors[prim['attributes']['POSITION']]
    view = views[pos['bufferView']]
    if pos['componentType'] != 5126 or pos['type'] != 'VEC3':
        raise ValueError('expected floating point positions')
    start = view.get('byteOffset', 0) + pos.get('byteOffset', 0)
    stride = view.get('byteStride', 12)
    joint_values, weight_values = [], []
    for vertex in range(pos['count']):
        y = struct.unpack_from('<fff', binary, start + vertex*stride)[1]
        second = blend(part, y)
        if primary[part] == secondary[part]:
            joint_values.extend([primary[part], 0, 0, 0])
            weight_values.extend([1, 0, 0, 0])
        else:
            first = 1-second
            joint_values.extend([primary[part] if first else 0,
                                 secondary[part] if second else 0, 0, 0])
            weight_values.extend([first, second, 0, 0])
    attrs = dict(prim['attributes'])
    attrs['JOINTS_0'] = add(joint_values, 'B', 'VEC4', 5121, pos['count'])
    attrs['WEIGHTS_0'] = add(weight_values, 'f', 'VEC4', 5126, pos['count'])
    new_prim = dict(prim)
    new_prim['attributes'] = attrs
    for accessor in attrs.values():
        views[accessors[accessor]['bufferView']]['target'] = 34962
    if 'indices' in prim:
        views[accessors[prim['indices']]['bufferView']]['target'] = 34963
    primitives.append(new_prim)

nodes = []
for bone, name in enumerate(names):
    parent = parents[bone]
    origin = pivots[parent] if parent >= 0 else (0,0,0)
    local = [pivots[bone][i]-origin[i] for i in range(3)]
    children = [i for i, value in enumerate(parents) if value == bone]
    node = {'name': name, 'translation': local}
    if children: node['children'] = children
    nodes.append(node)
nodes.append({'name': 'Character', 'mesh': 0, 'skin': 0})
inverse = []
for x,y,z in pivots:
    inverse.extend([1,0,0,0, 0,1,0,0, 0,0,1,0, -x,-y,-z,1])
bind_accessor = add(inverse, 'f', 'MAT4', 5126, len(names))
times = add([0,.25,.5,.75,1], 'f', 'SCALAR', 5126, 5)
accessors[times]['min'] = [0]
accessors[times]['max'] = [1]


def rotation(x, y, z):
    # Quaternion corresponding to the legacy Ry * Rx * Rz order.
    sx,cx = math.sin(x/2),math.cos(x/2)
    sy,cy = math.sin(y/2),math.cos(y/2)
    sz,cz = math.sin(z/2),math.cos(z/2)
    return [cy*sx*cz+sy*cx*sz, sy*cx*cz-cy*sx*sz,
            cy*cx*sz-sy*sx*cz, cy*cx*cz+sy*sx*sz]


def pose(state, phase):
    s = math.sin(phase)
    a = [[0.0,0.0,0.0] for _ in names]
    if state == 'Walk':
        a[6][0]=-.43*s; a[7][0]=.10+max(0,-s)*.12; a[8][0]=.04
        a[9][0]=.43*s; a[10][0]=.10+max(0,s)*.12; a[11][0]=.04
        a[12][0]=.58*s; a[13][0]=max(0,-s)*.55; a[14][0]=-.10*s
        a[15][0]=-.58*s; a[16][0]=max(0,s)*.55; a[17][0]=.10*s
    elif state == 'Jump':
        a[2][0]=-.08; a[6][0]=a[9][0]=-.72; a[7][0]=a[10][0]=-.12
        a[12][0]=-.28; a[13][0]=.20; a[15][0]=-.12; a[16][0]=.32
    elif state == 'Roll':
        a[2][0]=.35; a[2][2]=.48; a[6][0]=a[9][0]=-.55
        a[12][0]=a[15][0]=.45; a[13][0]=a[16][0]=-.65
    elif state == 'Slide':
        a[2][0]=.30; a[12][0]=a[15][0]=.55; a[13][0]=a[16][0]=-.78; a[6][0]=a[9][0]=-.28
    elif state == 'WallRun':
        a[2][0]=-.12; a[6][0]=-.85; a[9][0]=.72
        a[12][0]=.50+.18*s; a[13][0]=-.25; a[15][0]=-.35-.18*s; a[16][0]=.25
    elif state == 'Grapple':
        a[6][0]=a[9][0]=-.95; a[7][0]=a[10][0]=-.12; a[12][0]=.22; a[15][0]=-.22
    elif state == 'Vault':
        a[2][0]=-.18; a[6][0]=a[9][0]=-.78; a[12][0]=.48; a[13][0]=-.25; a[15][0]=-.38; a[16][0]=.18
    elif state == 'Attack':
        a[2][0]=-.06; a[6][0]=-.20; a[9][0]=.92; a[10][0]=-.18; a[11][0]=-.12
    elif state == 'Crouch':
        a[2][0]=.20; a[12][0]=a[15][0]=.35; a[13][0]=a[16][0]=-.62; a[6][0]=a[9][0]=.18
    a[18][1]=s*.025; a[19][0]=.08+s*.075; a[20][0]=.12+math.sin(phase+.4)*.16; a[20][1]=s*.055
    return a


animations = []
for state in ('Idle','Walk','Jump','Roll','Slide','WallRun','Grapple','Vault','Attack','Crouch'):
    poses = [pose(state, 2*math.pi*t) for t in (0,.25,.5,.75,1)]
    channels, samplers = [], []
    for bone in range(1, len(names)):
        values = [item for sample in poses for item in rotation(*sample[bone])]
        if all(abs(value - expected) < 1e-8 for sample in poses for value, expected in zip(sample[bone], (0,0,0))):
            continue
        output = add(values, 'f', 'VEC4', 5126, 5)
        samplers.append({'input': times, 'output': output, 'interpolation': 'LINEAR'})
        channels.append({'sampler': len(samplers)-1, 'target': {'node': bone, 'path': 'rotation'}})
    animations.append({'name': state, 'samplers': samplers, 'channels': channels})

while len(binary) % 4: binary.append(0)
document = {
    'asset': {'version': '2.0', 'generator': 'CodeRabbit prototype rig generator',
              'extras': {'coderabbit-ai-generated': 'v1', 'source': 'MUEANG_ISARA_Main_Assassin_02.glb'}},
    'scene': 0, 'scenes': [{'nodes': [0, len(names)]}], 'nodes': nodes,
    'meshes': [{'name': 'Mueang Assassin', 'primitives': primitives}],
    'skins': [{'name': 'Mueang prototype skeleton', 'joints': list(range(len(names))),
               'inverseBindMatrices': bind_accessor}],
    'animations': animations, 'accessors': accessors, 'bufferViews': views,
    'buffers': [{'byteLength': len(binary)}],
}
text = json.dumps(document, separators=(',', ':')).encode('utf-8')
text += b' ' * (-len(text) % 4)
total = 12 + 8 + len(text) + 8 + len(binary)
Path(sys.argv[2]).write_bytes(struct.pack('<4sII', b'glTF', 2, total)
                             + struct.pack('<I4s', len(text), b'JSON') + text
                             + struct.pack('<I4s', len(binary), b'BIN\0') + binary)
