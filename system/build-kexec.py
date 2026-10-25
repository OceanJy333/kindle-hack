#!/usr/bin/env python3
# input: pinned GPL source and measured stock config; output: temporary kexec module; pos: physical kernel bring-up.
from pathlib import Path
from sources import KERNEL_COMMIT, SOURCES
import hashlib
import json
import os
import subprocess
import uuid

SOURCE=Path(__file__).resolve().parent
WORK=SOURCE.parent/'build/independent-os'
ENV=os.environ.copy()
COMMIT = KERNEL_COMMIT


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def build():
    assert digest(WORK / SOURCES['kernel']['archive']) == SOURCES['kernel']['sha256']
    temporary=Path('/tmp')/('ocean-kexec-'+uuid.uuid4().hex)
    def execute(*arguments):
        return subprocess.check_output(['docker','exec','voyage-os-build',*map(str,arguments)],env=ENV,stderr=subprocess.STDOUT)
    def copy(source,target):
        subprocess.run(['docker','cp',str(source),'voyage-os-build:'+str(target)],env=ENV,check=True)
    execute('mkdir',temporary)
    try:
        execute('tar','xf','/work/linux-source.tar.gz','-C',temporary)
        kernel=temporary/('linux-3.0.35-lab126-'+COMMIT)
        output=temporary/'build';module=temporary/'module'
        execute('mkdir',output,module,module/'orig')
        copy(WORK/'private-backup/runtime-kernel.config',output/'.config')
        patch=temporary/'0001-modern-gcc-attributes.patch'
        copy(SOURCE/'patches'/patch.name,patch)
        execute('patch','-d',kernel,'-p1','-i',patch)
        for path in (SOURCE/'kexec').iterdir():copy(path,module/path.name)
        for source,target in [('kernel/kexec.c','orig/kexec.c'),
                              ('arch/arm/kernel/machine_kexec.c','orig/machine_kexec.c')]:
            execute('ln','-s',kernel/source,module/target)
        make=['make','-C',kernel,'O='+str(output),'ARCH=arm','CROSS_COMPILE=arm-linux-gnueabi-',
              'KCFLAGS=-fno-pie -fno-delete-null-pointer-checks']
        execute(*make,'oldnoconfig')
        execute(*make,'-j2','modules_prepare')
        stock=(WORK/'private-backup/runtime-kallsyms.txt').read_text()
        exports={line.split()[2][len('__ksymtab_'):] for line in stock.splitlines()
                 if len(line.split())>=3 and line.split()[2].startswith('__ksymtab_')}
        symbols='\n'.join(line for line in (WORK/'artifacts/Module.symvers').read_text().splitlines()
                          if len(line.split())>=2 and line.split()[1] in exports)+'\n'
        subprocess.run(['docker','exec','-i','voyage-os-build','python3','-c',
                        'from pathlib import Path;import sys;Path(sys.argv[1]).write_bytes(sys.stdin.buffer.read())',
                        str(output/'Module.symvers')],input=symbols.encode(),env=ENV,check=True)
        log=execute(*make,'M='+str(module),'-j2','modules')
        destination=WORK/'kexec';destination.mkdir(exist_ok=True)
        (destination/'build.log').write_bytes(log)
        subprocess.run(['docker','cp','voyage-os-build:'+str(module/'ocean-kexec.ko'),str(destination/'ocean-kexec.ko')],env=ENV,check=True)
        execute('arm-linux-gnueabihf-gcc','-O2','-static',module/'client.c','-o',module/'kexec-ctl')
        subprocess.run(['docker','cp','voyage-os-build:'+str(module/'kexec-ctl'),str(destination/'kexec-ctl')],env=ENV,check=True)
        imported=execute('arm-linux-gnueabi-nm','-u',module/'ocean-kexec.ko').decode().splitlines()
        imports=[line.split()[-1] for line in imported if line.strip()]
        missing=sorted(set(imports)-exports)
        release=execute(*make,'-s','kernelrelease').decode().splitlines()[-1]
        receipt={'kernelRelease':release,'sourceCommit':COMMIT,
                 'runtimeConfigSha256':digest(WORK/'private-backup/runtime-kernel.config'),
                 'moduleSha256':digest(destination/'ocean-kexec.ko'),
                 'imports':imports,'missingStockExports':missing,
                 'physicalModuleLoaded':False,'kernelJumpValidated':False}
        (destination/'manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
        print(json.dumps(receipt,indent=2))
        assert release=='3.0.35-lab126',release
        assert not missing,missing
    except subprocess.CalledProcessError as error:
        print(error.output.decode(errors='replace') if error.output else str(error),flush=True)
        raise
    finally:
        execute('python3','-c','import shutil,sys;shutil.rmtree(sys.argv[1])',temporary)


if __name__=='__main__':build()
