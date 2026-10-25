#!/usr/bin/env python3
# input: existing newc initramfs and replacement files; output: same archive with those entries swapped; pos: quick initramfs update without a full module rebuild.
import sys
from pathlib import Path

def entries(data):
    i=0
    while True:
        h=data[i:i+110];assert h[:6]==b'070701'
        f=[int(h[6+8*k:14+8*k],16) for k in range(13)]
        nsize,fsize=f[11],f[6];name=data[i+110:i+110+nsize-1].decode()
        j=i+110+nsize;j+=-j%4;body=data[j:j+fsize];end=j+fsize;end+=-end%4
        yield name,h,body
        if name=='TRAILER!!!':return
        i=end

def build(name,h,body):
    f=[int(h[6+8*k:14+8*k],16) for k in range(13)];f[6]=len(body)
    names=name.encode()+b'\0';f[11]=len(names)
    out=bytearray(b'070701'+''.join(f'{v:08x}' for v in f).encode()+names)
    out+=b'\0'*(-len(out)%4);out+=body;out+=b'\0'*(-len(out)%4);return bytes(out)

src,dst,*pairs=sys.argv[1:]
repl={pairs[k]:Path(pairs[k+1]).read_bytes() for k in range(0,len(pairs),2)}
out=bytearray();seen=set()
for name,h,body in entries(Path(src).read_bytes()):
    if name in repl:body=repl[name];seen.add(name)
    out+=build(name,h,body)
assert seen==set(repl),seen
Path(dst).write_bytes(out)
