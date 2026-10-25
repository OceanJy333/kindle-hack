#!/usr/bin/env python3
# input: initramfs files; output: root-owned newc archive; pos: reproducible RAM boot packaging.
from pathlib import Path
import stat


def archive(root,destination):
    output=bytearray()
    def entry(name,mode,content=b'',major=0,minor=0):
        names=name.encode()+b'\0'
        fields=[1,mode,0,0,1,0,len(content),0,0,major,minor,len(names),0]
        output.extend(b'070701'+''.join(f'{value:08x}' for value in fields).encode()+names)
        output.extend(b'\0'*(-len(output)%4));output.extend(content)
        output.extend(b'\0'*(-len(output)%4))
    for path in sorted(root.rglob('*')):
        name=path.relative_to(root).as_posix()
        if path.is_symlink():entry(name,stat.S_IFLNK|0o777,str(path.readlink()).encode())
        elif path.is_dir():entry(name,stat.S_IFDIR|0o755)
        elif path.is_file():entry(name,stat.S_IFREG|(path.stat().st_mode&0o777),path.read_bytes())
    entry('dev/console',stat.S_IFCHR|0o600,major=5,minor=1)
    entry('dev/null',stat.S_IFCHR|0o666,major=1,minor=3)
    entry('TRAILER!!!',0)
    destination.write_bytes(output)
