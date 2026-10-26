#!/usr/bin/env python3
# input: public runtime, games and installer; output: USB-ready ZIP and source bundle; pos: release packaging.
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def package(version):
    out = BUILD / 'release'
    stage = out / 'kindle-hack-ai-kit'
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    extension = stage / 'extensions/ocean'
    shutil.copytree(ROOT / 'installer/extension', extension)
    copy(BUILD / 'installer/device-probe', extension / 'bin/device-probe')
    shutil.copytree(BUILD / 'weread/native-runtime/public-runtime', extension / 'payload/runtime')
    for folder, names in [('doom', ['doom-voyage', 'overlay.gray']),
                          ('arcade', ['arcade-voyage', 'overlay.gray', 'missing.gray'])]:
        for name in names:
            copy(BUILD / folder / name, extension / 'payload/games' / folder / name)
    copy(ROOT / 'installer/install.py', stage / 'install.py')
    copy(ROOT / 'docs/给AI的安装任务.md', stage / '给AI的安装任务.md')
    copy(ROOT / 'README.md', stage / 'README.md')
    for name in ['AGENTS.md', 'BUILD.md', 'docs/setup.md', 'docs/给AI的安装任务.md', 'docs/release-validation.md',
                 '.agents/skills/kindle-setup/SKILL.md', 'weread/BUILD.md', 'system/BUILD.md']:
        copy(ROOT / name, stage / name)
    copy(ROOT / 'LICENSE', stage / 'LICENSE')
    copy(ROOT / 'THIRD_PARTY_NOTICES.md', stage / 'THIRD_PARTY_NOTICES.md')
    shutil.copytree(ROOT / 'LICENSES', stage / 'LICENSES')
    (stage / '源码与许可.txt').write_text(
        '对应源码附件：kindle-hack-sources.tar.gz。\n'
        'Qt/系统库的固定来源与获取方法见源码内 weread/BUILD.md 和 runtime/licenses。\n'
        '可替换 payload/runtime 下的动态库。账号、书籍、WAD、ROM 和 BIOS 不包含在发布包中。\n', encoding='utf-8')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    entries = {str(p.relative_to(stage)): sha(p) for p in sorted(stage.rglob('*')) if p.is_file()}
    manifest = {'version': version, 'sourceRevision': revision,
                'privateAssetsIncluded': False, 'logicalCanvas': [1072, 1448],
                'profiles': {'pw1': {'reader': False, 'games': 'device-probe-required'},
                             'pw2': {'reader': 'device-probe-required', 'games': 'device-probe-required'},
                             'pw3': {'reader': 'device-probe-required', 'games': 'device-probe-required'},
                             'voyage': {'reader': 'device-probe-required', 'games': 'device-probe-required'}},
                'hardwareValidation': 'pending', 'files': entries}
    (stage / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    archive = out / 'kindle-hack-ai-kit.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                z.write(path, path.relative_to(stage))
    sources = out / 'kindle-hack-sources.tar.gz'
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
    with tarfile.open(sources, 'w:gz', compresslevel=6) as tar:
        for name in tracked:
            if name and (ROOT / name).is_file():
                tar.add(ROOT / name, 'kindle-hack/' + name)
        for name in ['doom/doomgeneric.tar.gz', 'arcade/fba-source.tar.gz',
                     'native-deps/zlib.tar.gz', 'weread/native-runtime/glibc-2.39.tar.xz',
                     'weread/native-runtime/packages.sha256']:
            tar.add(BUILD / name, 'kindle-hack/build/' + name)
        tar.add(BUILD / 'weread/runtime-sources', 'kindle-hack/build/weread/runtime-sources')
    checksum = out / 'SHA256SUMS'
    checksum.write_text(''.join(f'{sha(p)}  {p.name}\n' for p in [archive, sources]))
    print(json.dumps({'archive': str(archive), 'bytes': archive.stat().st_size,
                      'sha256': sha(archive), 'sources': str(sources)}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', default='v0.2.0-ai-kit')
    package(parser.parse_args().version)
