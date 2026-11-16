"""Create a small self-contained GLB with two joints and a rotating clip."""
import json
import math
import struct
import sys

blob = bytearray()
views = []
accessors = []


def add(values, fmt, kind, component, count):
    while len(blob) % 4:
        blob.append(0)
    offset = len(blob)
    blob.extend(struct.pack('<' + fmt * len(values), *values))
    views.append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(blob) - offset})
    accessors.append({'bufferView': len(views) - 1, 'componentType': component,
                      'count': count, 'type': kind})
    return len(accessors) - 1


pos = add([0, 0, 0, 1, 0, 0, -.5, .2, 0], 'f', 'VEC3', 5126, 3)
norm = add([0, 0, 1] * 3, 'f', 'VEC3', 5126, 3)
joints = add([0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0], 'B', 'VEC4', 5121, 3)
weights = add([1, 0, 0, 0, 1, 0, 0, 0, .5, .5, 0, 0], 'f', 'VEC4', 5126, 3)
indices = add([0, 1, 2], 'H', 'SCALAR', 5123, 3)
identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
inverse_child = identity.copy()
inverse_child[13] = -1
inverse = add(identity + inverse_child, 'f', 'MAT4', 5126, 2)
times = add([0, 1], 'f', 'SCALAR', 5126, 2)
accessors[times]['min'] = [0]
accessors[times]['max'] = [1]
rotations = add([0, 0, 0, 1, 0, 0, math.sin(math.pi / 4), math.cos(math.pi / 4)],
                'f', 'VEC4', 5126, 2)
translations = add([0, 0, 0, .5, 0, 0], 'f', 'VEC3', 5126, 2)
while len(blob) % 4:
    blob.append(0)

doc = {
    'asset': {'version': '2.0', 'generator': 'Mueang test fixture'},
    'scene': 0, 'scenes': [{'nodes': [0, 2]}],
    'nodes': [{'name': 'Root', 'children': [1]},
              {'name': 'Child', 'translation': [0, 1, 0]},
              {'name': 'Mesh', 'mesh': 0, 'skin': 0}],
    'skins': [{'joints': [0, 1], 'inverseBindMatrices': inverse}],
    'meshes': [{'primitives': [{'attributes': {'POSITION': pos, 'NORMAL': norm,
                 'JOINTS_0': joints, 'WEIGHTS_0': weights}, 'indices': indices}]}],
    'animations': [{'name': 'Walk', 'samplers': [{'input': times, 'output': rotations}],
                    'channels': [{'sampler': 0, 'target': {'node': 1, 'path': 'rotation'}}]},
                   {'name': 'Idle', 'samplers': [{'input': times, 'output': translations}],
                    'channels': [{'sampler': 0, 'target': {'node': 0, 'path': 'translation'}}]}],
    'buffers': [{'byteLength': len(blob)}], 'bufferViews': views, 'accessors': accessors,
}
text = json.dumps(doc, separators=(',', ':')).encode('utf-8')
text += b' ' * (-len(text) % 4)
total = 12 + 8 + len(text) + 8 + len(blob)
with open(sys.argv[1], 'wb') as file:
    file.write(struct.pack('<4sII', b'glTF', 2, total))
    file.write(struct.pack('<I4s', len(text), b'JSON') + text)
    file.write(struct.pack('<I4s', len(blob), b'BIN\0') + blob)
