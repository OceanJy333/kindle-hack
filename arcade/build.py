#!/usr/bin/env python3
# input: preserved FBA source and Voyage adapters; output: silent static ARM Neo Geo frontend; pos: reproducible arcade build.
from pathlib import Path
import subprocess,tarfile,hashlib,json,shutil
root=Path(__file__).resolve().parents[1];adapter=Path(__file__).parent
lab=root/'build';out=lab/'arcade';src=out/'fbalpha2012_neogeo-master'
zig=lab/'native-deps/zig/zig'
if not src.is_dir():
    with tarfile.open(out/'fba-source.tar.gz') as a:a.extractall(out,filter='data')
core=src/'src/burner/libretro/libretro.cpp';code=core.read_text()
silent=code.replace('pBurnSoundOut = g_audio_buf;', 'pBurnSoundOut = NULL;').replace('pBurnSoundOut  = g_audio_buf;', 'pBurnSoundOut  = NULL;')
if silent!=code:core.write_text(silent)
sound=src/'src/burn/drv/neogeo/neo_run.cpp';code=sound.read_text()
silent=code.replace('\tBurnYM2610Update(pBurnSoundOut, nBurnSoundLen);','\tif (pBurnSoundOut) BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);')
if silent!=code:sound.write_text(silent)
code=core.read_text()
if 'frontend_av_enabled' not in code:
    old='   ForceFrameStep();\n\n   if (!nSkipFrame)'
    new='   unsigned frontend_av_enabled = 3;\n   if (environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &frontend_av_enabled) && !(frontend_av_enabled & 1)) nSkipFrame = 1;\n   ForceFrameStep();\n\n   if (!nSkipFrame)'
    assert code.count(old)==1
    core.write_text(code.replace(old,new))
subprocess.run(['node',str(adapter/'ui.ts')],check=True)
flags=['-target','arm-linux-musleabi','-mcpu=cortex_a9','-ffunction-sections','-fdata-sections']
with (out/'build.log').open('w') as log:
    common=['make','-f','makefile.libretro','platform=unix','STATIC_LINKING=1','GIT_VERSION= unknown']
    subprocess.run(common+['generate-files'],cwd=src,stdout=log,stderr=subprocess.STDOUT,check=True)
    # The generator includes the optional CD BIOS even when WANT_NEOGEOCD=0.
    drivers=src/'src/dep/generated/driverlist_neogeo.h'
    drivers.write_text(''.join(line for line in drivers.read_text().splitlines(True) if 'BurnDrvneocdz' not in line))
    (src/'src/burn/burn.o').unlink(missing_ok=True)
    subprocess.run(common+['-j','3','CC='+str(zig)+' cc '+' '.join(flags),'CXX='+str(zig)+' c++ -std=gnu++98 '+' '.join(flags),'AR='+str(zig)+' ar'],cwd=src,stdout=log,stderr=subprocess.STDOUT,check=True)
    inc=['-I',str(adapter),'-I',str(out),'-I',str(src/'src/burner/libretro')]
    objects=[]
    for name,path in [('frontend',adapter/'voyage.c'),('renderer',adapter/'video-render.c'),('display-worker',adapter.parent/'games/display-worker.c'),('input',adapter.parent/'doom/input.c'),('display',adapter.parent/'native/display.c')]:
        obj=out/(name+'.o');subprocess.run([str(zig),'cc',*flags,'-O2',*inc,'-c',str(path),'-o',str(obj)],stdout=log,stderr=subprocess.STDOUT,check=True);objects.append(str(obj))
    # Upstream gives its static archive a .so suffix; Zig uses the suffix to choose link mode.
    archive=out/'core.a';shutil.copyfile(src/'fbalpha2012_neogeo_libretro.so',archive)
    subprocess.run([str(zig),'c++',*flags,'-O2','-static','-s','-Wl,--gc-sections',*objects,str(archive),'-lm','-o',str(out/'arcade-voyage')],stdout=log,stderr=subprocess.STDOUT,check=True)
print(json.dumps({'executable':str(out/'arcade-voyage'),'sha256':hashlib.sha256((out/'arcade-voyage').read_bytes()).hexdigest(),'core':'FB Alpha 2012 Neo Geo','armNativeFrontend':True,'neoGeoEmulation':True,'audioDevice':False}))
