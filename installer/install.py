#!/usr/bin/env python3
# input: release extension and mounted Kindle; output: USB install/status; pos: host entry.
"""Copy the ready-made release; no SSH, compiler or device configuration needed."""
import argparse
import json
from pathlib import Path
import shutil
import sys

REQUIRED = (
    'config.xml', 'menu.json', 'bin/ocean.sh', 'bin/detect.sh', 'bin/launch.sh',
    'bin/device-probe', 'payload/runtime/ld-linux-armhf.so.3',
    'payload/runtime/runtime-check', 'payload/runtime/weread-login',
    'payload/runtime/ca-certificates.crt', 'payload/runtime/ReaderApp.qml',
    'payload/runtime/fonts/NotoSansCJKsc-Regular.otf',
    'payload/games/doom/doom-voyage', 'payload/games/doom/overlay.gray',
    'payload/games/arcade/arcade-voyage', 'payload/games/arcade/overlay.gray',
    'payload/games/arcade/missing.gray',
)
NEXT = '安全弹出 Kindle → 拔下 USB → 打开 KUAL → Ocean → 安装 / 检查'


def extension_source(explicit=None):
    if explicit:
        return Path(explicit).expanduser().resolve()
    home = Path(__file__).resolve().parent
    for release in (home / 'extensions/ocean', home.parent / 'extensions/ocean'):
        if release.is_dir():
            return release
    return home / 'extension'


def find_kindle(explicit=None):
    if explicit:
        target = Path(explicit).expanduser().resolve()
        if not target.is_dir():
            raise ValueError(f'Kindle 目录不存在：{target}')
        return target
    candidates = [Path('/Volumes/Kindle')]
    for base in (Path('/media'), Path('/run/media'), Path('/mnt')):
        if base.is_dir():
            candidates.extend(base.glob('*/Kindle'))
            candidates.append(base / 'Kindle')
    found = sorted({p.resolve() for p in candidates if p.is_dir()})
    if len(found) != 1:
        raise ValueError('请指定 Kindle USB 根目录：python3 installer/install.py --kindle /Volumes/Kindle')
    return found[0]


def missing_files(extension):
    return [name for name in REQUIRED if not (extension / name).is_file()]


def read_status(kindle):
    path = kindle / 'ocean/state/status.json'
    if path.is_file():
        return json.loads(path.read_text())
    return {'phase': 'awaiting_device_check'}


def run(command, kindle, source, progress=False):
    target = kindle / 'extensions/ocean'
    if command == 'install':
        missing = missing_files(source)
        if missing:
            raise ValueError('发行包不完整：' + ', '.join(missing))
        if target.is_symlink():
            raise ValueError('extensions/ocean 是符号链接，请先选择实际 USB 根目录')
        size = sum(p.stat().st_size for p in source.rglob('*') if p.is_file())
        old_size = sum(p.stat().st_size for p in target.rglob('*') if p.is_file()) if target.exists() else 0
        if size > shutil.disk_usage(kindle).free + old_size:
            raise ValueError(f'Kindle 空间不足：需要 {size // 1048576 + 1} MiB')
        # Only our extension is replaced. Accounts, offline books and saves live outside it.
        if source.resolve() != target.resolve():
            if target.exists():
                shutil.rmtree(target)
            copied = 0
            reported = -1

            def copy_file(src, dst):
                nonlocal copied, reported
                result = shutil.copy2(src, dst)
                copied += Path(src).stat().st_size
                percentage = int(copied * 100 / max(size, 1))
                if progress and (percentage // 10 != reported or copied == size):
                    print(f'复制到 Kindle：{percentage}%', file=sys.stderr, flush=True)
                    reported = percentage // 10
                return result

            if progress:
                print(f'正在复制 {size // 1048576 + 1} MiB 到 {kindle}', file=sys.stderr, flush=True)
            shutil.copytree(source, target, copy_function=copy_file)
        for path in (target / 'bin').iterdir():
            if path.is_file():
                path.chmod(path.stat().st_mode | 0o111)
        for path in (target / 'payload').rglob('*'):
            if path.is_file() and (path.suffix == '.sh' or path.name in (
                    'ld-linux-armhf.so.3', 'runtime-check', 'weread-login',
                    'doom-voyage', 'arcade-voyage')):
                path.chmod(path.stat().st_mode | 0o111)
        state = kindle / 'ocean/state'
        state.mkdir(parents=True, exist_ok=True)
        (state / 'status.json').write_text('{"phase":"awaiting_device_check"}\n')
        return {'phase': 'copied', 'kindle': str(kindle), 'next': NEXT}
    if command == 'uninstall':
        if target.is_symlink():
            target.unlink()
        elif target.exists():
            shutil.rmtree(target)
        state = kindle / 'ocean/state'
        state.mkdir(parents=True, exist_ok=True)
        (state / 'status.json').write_text('{"phase":"uninstalled","dataPreserved":true}\n')
        return {'phase': 'uninstalled', 'preserved': ['ocean', 'games', 'weread-offline']}
    result = {'extensionPresent': target.is_dir(), 'device': read_status(kindle)}
    if command == 'verify':
        result['missing'] = missing_files(target)
        result['ok'] = not result['missing']
        result['next'] = NEXT
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', nargs='?', default='install',
                        choices=['install', 'status', 'verify', 'uninstall'])
    parser.add_argument('--kindle')
    parser.add_argument('--source', help=argparse.SUPPRESS)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    try:
        result = run(args.command, find_kindle(args.kindle), extension_source(args.source), progress=True)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        if not args.json and 'next' in result:
            print(result['next'])
        return 1 if result.get('ok') is False else 0
    except (ValueError, OSError, json.JSONDecodeError) as error:
        print(json.dumps({'error': str(error)}, ensure_ascii=False), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
