#!/usr/bin/env python3
# input: Debian archives or retained SHA-256 list; output: source/library provenance JSON; pos: runtime release evidence.
"""Run before deleting downloaded .deb files; checksum-only mode records missing metadata."""
import argparse
import hashlib
import json
import re
import subprocess
import tarfile
from pathlib import Path
from urllib.parse import unquote


def package_identity(filename):
    package, version, architecture = unquote(filename).removesuffix('.deb').rsplit('_', 2)
    return {'package': package, 'version': version, 'architecture': architecture}


def source_identity(package, version, source):
    if not source:
        return {'package': package, 'version': version}
    match = re.fullmatch(r'([^\s]+)(?:\s+\(([^)]+)\))?', source)
    if not match:
        raise ValueError(f'Invalid Debian Source field: {source!r}')
    return {'package': match[1], 'version': match[2] or version}


def archive_metadata(path):
    result = subprocess.run(['dpkg-deb', '--field', str(path)], check=True, capture_output=True, text=True)
    fields = {}
    for line in result.stdout.splitlines():
        if line and not line[0].isspace() and ': ' in line:
            name, value = line.split(': ', 1)
            fields[name] = value
    libraries, copyrights = [], []
    process = subprocess.Popen(['dpkg-deb', '--fsys-tarfile', str(path)], stdout=subprocess.PIPE)
    try:
        with tarfile.open(fileobj=process.stdout, mode='r|*') as archive:
            for item in archive:
                name = '/' + item.name.removeprefix('./').lstrip('/')
                if not item.isdir() and re.search(r'\.so(?:\.[^/]+)*$', name):
                    libraries.append(name)
                if name.startswith('/usr/share/doc/') and name.endswith('/copyright'):
                    copyrights.append(name)
    finally:
        process.stdout.close()
        if process.wait():
            raise RuntimeError(f'Cannot list Debian archive: {path}')
    return {
        'package': fields['Package'], 'version': fields['Version'], 'architecture': fields['Architecture'],
        'source': source_identity(fields['Package'], fields['Version'], fields.get('Source', '')),
        'sourceField': fields.get('Source'), 'metadataAvailable': True, 'controlMetadataAvailable': True,
        'sharedObjects': sorted(libraries), 'copyrightFiles': sorted(copyrights),
    }


def checksum_inventory(path):
    entries = []
    for line in path.read_text().splitlines():
        checksum, filename = line.split(maxsplit=1)
        filename = filename.lstrip('*')
        entries.append({**package_identity(filename), 'archive': filename, 'sha256': checksum,
                        'source': None, 'sourceField': None, 'metadataAvailable': False, 'controlMetadataAvailable': False,
                        'sharedObjects': None, 'copyrightFiles': None})
    return entries


def restore_controls(packages, path):
    controls = {}
    for paragraph in path.read_text().split('\n\n'):
        fields = dict(line.split(': ', 1) for line in paragraph.splitlines()
                      if line and not line[0].isspace() and ': ' in line)
        if all(name in fields for name in ('Package', 'Version', 'Architecture')):
            controls[(fields['Package'], fields['Version'], fields['Architecture'])] = fields
    for package in packages:
        key = (package['package'], package['version'], package['architecture'])
        fields = controls.get(key)
        if fields is None:
            raise ValueError(f'Exact control record missing: {key}')
        if fields.get('SHA256') != package['sha256']:
            raise ValueError(f'Archive SHA-256 differs from snapshot control: {key}')
        package.update({
            'source': source_identity(*key[:2], fields.get('Source', '')),
            'sourceField': fields.get('Source'), 'controlMetadataAvailable': True,
            'binaryPath': fields['Filename'], 'binarySize': int(fields['Size']),
        })


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--deb-dir', type=Path)
    inputs.add_argument('--checksums', type=Path)
    parser.add_argument('--snapshot', default='20260201T000000Z')
    controls_group = parser.add_mutually_exclusive_group()
    controls_group.add_argument('--controls', type=Path, help='Exact-version apt-cache show output, including SHA256.')
    controls_group.add_argument('--query-apt-cache', action='store_true', help='Save exact-version apt-cache controls before apt lists are removed.')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.deb_dir:
        archives = sorted(args.deb_dir.glob('*.deb'))
        if not archives:
            parser.error(f'No Debian archives in {args.deb_dir}')
        packages = [{**archive_metadata(path), 'archive': path.name,
                     'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for path in archives]
    else:
        packages = checksum_inventory(args.checksums)
    controls = args.controls
    if args.query_apt_cache:
        specs = [f"{p['package']}:{p['architecture']}={p['version']}" for p in packages]
        result = subprocess.run(['apt-cache', 'show', *specs], check=True, capture_output=True, text=True)
        controls = args.output.with_suffix('.controls.txt')
        controls.parent.mkdir(parents=True, exist_ok=True)
        controls.write_text(result.stdout)
    if controls:
        restore_controls(packages, controls)
    objects = {}
    for package in packages:
        for path in package['sharedObjects'] or []:
            objects.setdefault(path, []).append({'package': package['package'], 'version': package['version'],
                                                'architecture': package['architecture'], 'source': package['source']})
    result = {
        'schemaVersion': 1, 'snapshot': args.snapshot,
        'repositories': ['https://snapshot.debian.org/archive/debian/' + args.snapshot + '/',
                         'https://snapshot.debian.org/archive/debian-security/' + args.snapshot + '/'],
        'metadataComplete': all(package['metadataAvailable'] for package in packages),
        'sourceMetadataComplete': all(package['controlMetadataAvailable'] for package in packages),
        'controlEvidence': controls.name if controls else None,
        'missingMetadata': None if args.deb_dir else ('Archive file lists are missing; exact-version controls were recovered and SHA-256 matched.' if controls else 'Archives were removed before control fields and file lists were saved.'),
        'packages': packages, 'sharedObjectOwners': dict(sorted(objects.items())),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps({'output': str(args.output), 'packages': len(packages), 'sharedObjects': len(objects),
                      'metadataComplete': result['metadataComplete'], 'sourceMetadataComplete': result['sourceMetadataComplete']}))


if __name__ == '__main__':
    main()
