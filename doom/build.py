#!/usr/bin/env python3
# input: Doomgeneric source and Voyage adapter; output: native static ARM game; pos: reproducible low-memory build.
from pathlib import Path
import subprocess,shlex,json,hashlib,tarfile
root=Path(__file__).resolve().parents[1]
adapter=Path(__file__).parent
lab=root/'build';out=lab/'doom';src=out/'doomgeneric-master/doomgeneric'
if not src.is_dir():
    with tarfile.open(out/'doomgeneric.tar.gz') as archive:archive.extractall(out,filter='data')
subprocess.run(['node',str(adapter/'ui.ts')],check=True)
line=next(x for x in (src/'Makefile').read_text().splitlines() if x.startswith('SRC_DOOM ='))
names=[x.removesuffix('.o')+'.c' for x in shlex.split(line.split('=',1)[1]) if x!='doomgeneric_xlib.o']
command=[str(lab/'native-deps/zig/zig'),'cc','-target','arm-linux-musleabi','-mcpu=cortex_a9','-O2','-static','-s','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fno-strict-aliasing','-fwrapv','-DNORMALUNIX','-DLINUX','-DSNDSERV','-D_DEFAULT_SOURCE','-DDOOMGENERIC_RESX=320','-DDOOMGENERIC_RESY=200','-I',str(src),'-I',str(out)]
command += [str(src/x) for x in names]+[str(adapter/'voyage.c'),str(adapter/'input.c'),str(adapter.parent/'native/display.c'),str(adapter.parent/'games/display-worker.c'),'-lm','-o',str(out/'doom-voyage')]
with (out/'build.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
print(json.dumps({'executable':str(out/'doom-voyage'),'sha256':hashlib.sha256((out/'doom-voyage').read_bytes()).hexdigest(),'cpuTranslation':False,'audio':False}))
