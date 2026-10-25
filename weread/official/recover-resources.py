#!/usr/bin/env python3
# input: user's RemarkableWeRead 1.0.0 ELF; output: local embedded QML/assets; pos: offline build-input extraction.
"""Extract the pinned client's Qt RCC arrays without starting its executable."""
from pathlib import Path
import argparse
import ctypes
import ctypes.util
import hashlib
import json
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = '5a117d08c8503bad49b65a54357cdc1698b0ae3e52550d704fe9c6262f061ac7'
# Version-3 RCC tree/name/data virtual addresses at qRegisterResourceData callers.
TABLES = [(0x2f6930, 0x2f68e0, 0x2f6990),
          (0x2fd710, 0x2fd6c0, 0x2fd570),
          (0x34fec0, 0x34f9a0, 0x350210),
          (0x361d10, 0x361c90, 0x361c10)]


def extract(binary, output):
    data = binary.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED:
        raise ValueError(f'Expected RemarkableWeRead 1.0.0 ELF {EXPECTED}; got {digest}')
    header = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
    segments = [struct.unpack_from('<IIQQQQQQ', data, header[5] + i * header[9])
                for i in range(header[10])]

    def offset(address):
        for kind, _, start, base, _, size, _, _ in segments:
            if kind == 1 and base <= address < base + size:
                return start + address - base
        raise ValueError(f'Unmapped ELF address: {address:x}')

    def be16(start):
        return int.from_bytes(data[start:start + 2], 'big')

    def be32(start):
        return int.from_bytes(data[start:start + 4], 'big')

    zstd = ctypes.CDLL(ctypes.util.find_library('zstd') or 'libzstd.so.1')
    zstd.ZSTD_getFrameContentSize.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    zstd.ZSTD_getFrameContentSize.restype = ctypes.c_ulonglong
    zstd.ZSTD_decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
    zstd.ZSTD_decompress.restype = ctypes.c_size_t
    files = {}

    def walk(tree, names, payload, node=0, parent=''):
        entry = tree + node * 22
        name = names + be32(entry)
        text = data[name + 6:name + 6 + be16(name) * 2].decode('utf-16-be') if node else ''
        path = parent + '/' + text if node else ''
        flags = be16(entry + 4)
        if flags & 2:
            for child in range(be32(entry + 10), be32(entry + 10) + be32(entry + 6)):
                walk(tree, names, payload, child, path)
            return
        prefix = '/qt/qml/remarkable_weread/'
        if not path.startswith(prefix):
            return
        start = payload + be32(entry + 10)
        content = data[start + 4:start + 4 + be32(start)]
        if flags & 4:
            source = ctypes.create_string_buffer(content)
            length = zstd.ZSTD_getFrameContentSize(source, len(content))
            target = ctypes.create_string_buffer(length)
            if zstd.ZSTD_decompress(target, length, source, len(content)) != length:
                raise ValueError(f'Cannot decompress {path}')
            content = target.raw
        elif flags & 1:
            content = zlib.decompress(content[4:])
        relative = path[len(prefix):]
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
        files[relative] = hashlib.sha256(content).hexdigest()

    for table in TABLES:
        walk(*(offset(address) for address in table))
    if 'qml/pages/ReaderPage.qml' not in files:
        raise ValueError('ReaderPage.qml was not found in this package')
    receipt = {'inputSha256': digest, 'files': files}
    (output.parent / 'resource-hashes.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'Extracted {len(files)} resources into {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path, help='User-supplied payload/bin/remarkable-weread')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/weread/official-recovered-resources')
    args = parser.parse_args()
    extract(args.binary, args.output)
