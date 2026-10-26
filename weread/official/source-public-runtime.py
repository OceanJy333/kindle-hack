#!/usr/bin/env python3
# input: built reader, sysroot and pinned package indices; output: verified corresponding source files; pos: public release provenance.
import argparse
import gzip
import hashlib
import io
import json
import posixpath
import shutil
import subprocess
import tarfile
from pathlib import Path
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'build/weread/native-runtime'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def download(url, target, expected=None):
    if not target.is_file() or expected and digest(target) != expected:
        target.parent.mkdir(parents=True, exist_ok=True)
        temp = target.with_name(target.name + '.part')
        result = subprocess.run(['curl', '-fsSL', '--retry', '2', '--max-time', '180', url, '-o', str(temp)])
        if result.returncode:
            temp.unlink(missing_ok=True)
            return False
        temp.replace(target)
    if expected and digest(target) != expected:
        raise ValueError(f'SHA-256 mismatch: {target}')
    return True


def fetch_any(repositories, path, target, expected=None):
    evidence = target.with_name(target.name + '.url')
    if target.is_file() and evidence.is_file() and (not expected or digest(target) == expected):
        return evidence.read_text().strip()
    for repository in repositories:
        url = repository + quote(path, safe='/+')
        if target.is_file() and not evidence.is_file():
            target.unlink()
        if download(url, target, expected):
            evidence.write_text(url + '\n')
            return url
    raise RuntimeError(f'Unavailable in pinned repositories: {path}')


def runtime_closure():
    stage = BUILD / 'public-runtime'
    manifest = json.loads((stage / 'runtime-manifest.json').read_text())
    base = BUILD / 'debian-root'
    libs = base / 'usr/lib/arm-linux-gnueabihf'
    paths = set()
    # The assembled runtime is the authority; do not maintain a second plugin list.
    for name in manifest['libraries']:
        path = libs / ('qt6/' + name if name.startswith('plugins/') else name)
        if not path.is_file():
            path = base / 'lib/arm-linux-gnueabihf' / name
        if not path.is_file():
            raise FileNotFoundError(path)
        paths.add(path)
    modules = stage / 'qml-modules'
    for path in modules.rglob('*.so'):
        paths.add(libs / 'qt6/qml' / path.relative_to(modules))
    return sorted(path.relative_to(base).as_posix() for path in paths)


def contents_owners(paths):
    owners = {}
    pending = set(paths)
    with gzip.open(BUILD / 'Contents-armhf.gz', 'rt') as stream:
        for line in stream:
            path, names = line.rstrip().rsplit(None, 1)
            if path in pending:
                owners[path] = [name.rsplit('/', 1)[-1] for name in names.split(',')]
    return owners


def deb_archive(path):
    data = path.read_bytes()
    if data[:8] != b'!<arch>\n':
        raise ValueError(f'Invalid Debian archive: {path}')
    offset = 8
    while offset < len(data):
        header = data[offset:offset + 60]
        name, size = header[:16].decode().strip().rstrip('/'), int(header[48:58])
        offset += 60
        if name.startswith('data.tar'):
            return tarfile.open(fileobj=io.BytesIO(data[offset:offset + size]), mode='r:*')
        offset += size + size % 2
    raise ValueError(f'Debian data archive missing: {path}')


def archive_file_hash(archive, name):
    members = {item.name.removeprefix('./').lstrip('/'): item for item in archive.getmembers()}
    member = members[name]
    while member.issym() or member.islnk():
        target = member.linkname
        if member.issym() and not target.startswith('/'):
            target = posixpath.join(posixpath.dirname(member.name), target)
        member = members[posixpath.normpath(target).removeprefix('./').lstrip('/')]
    with archive.extractfile(member) as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def source_files(dsc):
    lines = dsc.read_text().splitlines()
    start = lines.index('Checksums-Sha256:') + 1
    files = []
    for line in lines[start:]:
        if not line.startswith(' '):
            break
        checksum, size, name = line.split()
        files.append({'name': name, 'size': int(size), 'sha256': checksum})
    if not files:
        raise ValueError(f'Empty source checksum list: {dsc}')
    return files


def validate_stage(files):
    stage = BUILD / 'public-runtime'
    if not stage.is_dir():
        return False
    expected = {'weread-login', 'runtime-check'}
    for file in files:
        path = file['path']
        if '/qt6/qml/' in path:
            relative = 'qml-modules/' + path.split('/qt6/qml/', 1)[1]
        elif '/qt6/plugins/' in path:
            relative = 'plugins/' + path.split('/qt6/plugins/', 1)[1]
        else:
            relative = Path(path).name
        if digest(stage / relative) != file['sha256']:
            raise ValueError(f'Public runtime SHA-256 mismatch: {relative}')
        file['runtimePath'] = relative
        expected.add(relative)
    actual = {path.relative_to(stage).as_posix() for path in stage.rglob('*')
              if path.is_file() and path.open('rb').read(4) == b'\x7fELF'}
    if actual != expected:
        raise ValueError(f'Runtime ELF coverage mismatch: {actual ^ expected}')
    return True


def collect(output, fetch):
    index = json.loads((BUILD / 'debian-package-index.json').read_text())
    supplement_path = BUILD / 'supplement-index.json'
    supplement = json.loads(supplement_path.read_text()) if supplement_path.is_file() else {'packages': []}
    packages = {p['package']: p for p in index['packages'] + supplement['packages']
                if p['architecture'] in ('armhf', 'all')}
    repositories = index['repositories']
    paths = runtime_closure()
    owners = {}
    for name, package in packages.items():
        for path in package['sharedObjects'] or []:
            owners.setdefault(path.lstrip('/'), []).append(name)
    missing = set(paths) - owners.keys()
    if missing:
        contents = BUILD / 'Contents-armhf.gz'
        if not contents.is_file():
            fetch_any(repositories[:1], 'dists/bookworm/main/Contents-armhf.gz', contents)
        for path, names in contents_owners(missing).items():
            owners.setdefault(path, []).extend(names)
    files, selected = [], {}
    for path in paths:
        candidates = sorted(set(owners.get(path, [])) & packages.keys())
        if len(candidates) != 1:
            raise ValueError(f'Ambiguous/missing package owner: {path}: {candidates}')
        package = packages[candidates[0]]
        is_glibc = package['source']['package'] == 'glibc'
        files.append({'path': path, 'sha256': digest(BUILD / 'debian-root' / path),
                      'binaryPackage': package['package'], 'binaryVersion': package['version'],
                      'source': {'package': 'glibc', 'version': '2.39', 'origin': 'GNU upstream plus build-native-libc.sh patches'} if is_glibc else package['source'], 'patchedGlibcReplacement': is_glibc})
        if package['source']['package'] != 'glibc':
            selected[package['package']] = package
    output.mkdir(parents=True, exist_ok=True)
    binary_dir = BUILD / 'runtime-source-debs'
    binary_dir.mkdir(exist_ok=True)
    sources = {}
    for name, package in sorted(selected.items()):
        local = BUILD / 'supplement' / package['archive']
        target = local if local.is_file() else binary_dir / package['archive']
        url = None
        if local.is_file():
            if digest(local) != package['sha256']:
                raise ValueError(f'Supplement SHA-256 mismatch: {local}')
        else:
            url = fetch_any(repositories, package['binaryPath'], target, package['sha256'])
        with deb_archive(target) as archive:
            for file in files:
                if file['binaryPackage'] == name and archive_file_hash(archive, file['path']) != file['sha256']:
                    raise ValueError(f'Runtime differs from exact Debian archive: {file["path"]}')
        copyright_file = BUILD / 'debian-root/usr/share/doc' / name / 'copyright'
        if not copyright_file.is_file():
            raise FileNotFoundError(copyright_file)
        notice = output / 'licenses' / name / 'copyright'
        notice.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(copyright_file, notice)
        package['copyright'] = {'path': str(notice.relative_to(output)), 'sha256': digest(notice)}
        package['verifiedArchive'] = str(target.relative_to(ROOT))
        package['downloadUrl'] = url
        source = package['source']
        key = (source['package'], source['version'])
        if key not in sources:
            representative = next(p for p in index['packages'] if p['source'] == source and p.get('binaryPath'))
            directory = str(Path(representative['binaryPath']).parent)
            dsc_name = source['package'] + '_' + source['version'].split(':')[-1] + '.dsc'
            folder = output / source['package']
            dsc = folder / dsc_name
            dsc_url = fetch_any(repositories, directory + '/' + dsc_name, dsc)
            sources[key] = {**source, 'dsc': str(dsc.relative_to(output)), 'dscUrl': dsc_url,
                            'dscSha256': digest(dsc), 'directory': directory, 'files': source_files(dsc)}
    for file in files:
        if file['patchedGlibcReplacement']:
            file['sysrootCandidateSha256'] = file.pop('sha256')
            file['binaryPackage'] = file['binaryVersion'] = None
            replacement = next((path for path in (BUILD / 'glibc-install').rglob(Path(file['path']).name) if path.is_file()), None)
            file['sha256'] = digest(replacement) if replacement else None
    stage_validated = validate_stage(files)
    total = sum(f['size'] for source in sources.values() for f in source['files'])
    bundle_bytes = total + (BUILD / 'glibc-2.39.tar.xz').stat().st_size
    result = {'snapshot': index['snapshot'], 'runtimeStageValidated': stage_validated, 'runtimeFiles': files, 'binaryPackages': list(selected.values()),
              'sources': list(sources.values()), 'sourceDownloadBytes': total, 'sourceArchiveBytes': bundle_bytes,
              'patchedGlibc': {'version': '2.39', 'archiveSha256': 'f77bd47cf8170c57365ae7bf86696c118adb3b120d3259c64c502d3dc1e2d926',
                               'upstreamUrl': 'https://ftp.gnu.org/gnu/glibc/glibc-2.39.tar.xz', 'patchAndBuildScript': 'build-native-libc.sh'}, 'sourcesDownloaded': False}
    manifest = output / 'source-manifest.json'
    manifest.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'runtimeFiles': len(files), 'binaryPackages': len(selected), 'sources': len(sources), 'sourceDownloadBytes': total}), flush=True)
    if fetch:
        if bundle_bytes > 500 * 1024 * 1024:
            raise ValueError('Sources exceed 500 MiB; review source-manifest.json before downloading.')
        for source in sources.values():
            for file in source['files']:
                target = output / source['package'] / file['name']
                file['url'] = fetch_any(repositories, source['directory'] + '/' + file['name'], target, file['sha256'])
        glibc = BUILD / 'glibc-2.39.tar.xz'
        if digest(glibc) != result['patchedGlibc']['archiveSha256']:
            raise ValueError('Patched glibc upstream archive SHA-256 mismatch')
        shutil.copyfile(glibc, output / glibc.name)
        notice_dir = output / 'licenses/glibc'
        notice_dir.mkdir(parents=True, exist_ok=True)
        with tarfile.open(glibc) as archive:
            for name in ['COPYING', 'COPYING.LIB', 'LICENSES']:
                member = archive.getmember('glibc-2.39/' + name)
                (notice_dir / name).write_bytes(archive.extractfile(member).read())
        shutil.copyfile(Path(__file__).parent / 'build-native-libc.sh', output / 'build-native-libc.sh')
        font = json.loads((ROOT / 'tools/dependencies.json').read_text())['font-Regular']
        font_url = font['url'].split('/Sans/', 1)[0] + '/LICENSE'
        font_notice = output / 'licenses/Noto-OFL.txt'
        if not download(font_url, font_notice):
            raise RuntimeError('Unable to fetch pinned Noto license')
        stage = BUILD / 'public-runtime'
        if stage.is_dir() and digest(stage / font['file']) != font['sha256']:
            raise ValueError('Public Noto font differs from pinned dependency')
        ca = output / 'licenses/ca-certificates/copyright'
        ca.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(BUILD / 'debian-root/usr/share/doc/ca-certificates/copyright', ca)
        result['additionalLicenses'] = {
            'font': {**font, 'license': str(font_notice.relative_to(output)), 'licenseUrl': font_url, 'licenseSha256': digest(font_notice)},
            'certificates': {'source': packages['ca-certificates']['source'], 'license': str(ca.relative_to(output)),
                             'licenseSha256': digest(ca), 'bundleSha256': digest(BUILD / 'ca-certificates.crt')},
        }
        result['sourcesDownloaded'] = True
        manifest.write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/weread/runtime-sources')
    parser.add_argument('--fetch', action='store_true', help='Fetch source attachments after computing and checking the total size.')
    args = parser.parse_args()
    collect(args.output, args.fetch)
