#!/usr/bin/env python3
# input: paired Voyage before flashing; output: verified private hardware snapshot; pos: device-specific firmware provisioning.
from pathlib import Path
import gzip
import hashlib
import io
import json
import os
import posixpath
import struct
import subprocess
import tarfile
import zlib

SOURCE = Path(__file__).resolve().parent
WORK = SOURCE.parent / 'build/independent-os'


def waveform(raw):
    if len(raw) < 48:
        raise ValueError('Truncated waveform header')
    checksum, size = struct.unpack_from('<II', raw)
    if not 48 < size <= min(len(raw), 8 * 1024 * 1024):
        raise ValueError('Invalid waveform size')
    payload = raw[:size]
    actual = zlib.crc32(b'\0' * 4 + payload[4:])
    if actual != checksum:
        raise ValueError('Panel waveform CRC mismatch')
    return payload


def fetch(command):
    return subprocess.check_output(['sh', str(SOURCE / 'device.sh'), command], timeout=90)


def capture():
    os.umask(0o077)
    payload = waveform(fetch('cat /proc/eink/panel/waveform/data'))
    compressed = gzip.compress(payload, compresslevel=6, mtime=0)
    archive = fetch('tar -czf - -C / lib/firmware opt/ar6k/target/AR6003/hw2.1.1/bin')
    # Verify the gzip stream before parsing or retaining a transfer.
    plain = gzip.decompress(archive)
    normalized = io.BytesIO()
    members = []
    with tarfile.open(fileobj=io.BytesIO(plain)) as source, tarfile.open(fileobj=normalized,mode='w:gz') as target:
        for member in source:
            member.uid=member.gid=0
            member.uname=member.gname='root'
            if member.issym() and member.linkname.startswith('/'):
                member.linkname=posixpath.relpath(member.linkname.lstrip('/'),posixpath.dirname(member.name))
            if member.isfile():
                with source.extractfile(member) as stream:
                    target.addfile(member,stream)
                members.append(member.name)
            else:target.addfile(member)
    archive=normalized.getvalue()
    assert any('athwlan.bin' in name or 'fw-4.bin' in name for name in members)
    destination = WORK / 'private-backup/hardware'
    destination.mkdir(mode=0o700, parents=True, exist_ok=True)
    for name, data in [('panel.wrf.gz', compressed), ('firmware.tar.gz', archive)]:
        temporary = destination / (name + '.partial')
        temporary.write_bytes(data)
        temporary.replace(destination / name)
    receipt = {'waveformBytes': len(payload), 'waveformCrcVerified': True,
               'waveformSha256': hashlib.sha256(payload).hexdigest(),
               'firmwareArchiveSha256': hashlib.sha256(archive).hexdigest(),
               'firmwareFiles': members, 'provisionedOnNewSystem': False}
    (destination / 'manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'waveformBytes': len(payload), 'firmwareFiles': len(members), 'crcVerified': True}))


if __name__ == '__main__':
    capture()
