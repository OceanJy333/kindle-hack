#!/usr/bin/env python3
# input: Voyage QEMU binary; output: DMA setup regression evidence; pos: storage-model validation.
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile
from boot_console import connect

SOURCE=Path(__file__).resolve().parent
REPO=SOURCE.parent
QEMU=REPO/'build/tools/eink-emulator/qemu/build/qemu-system-arm'
EVIDENCE=SOURCE.parent/'build/reports/independent-os'


def verify():
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ocean-storage-') as folder:
        temporary=Path(folder)
        disk=temporary/'disk.raw'
        with disk.open('wb') as stream:stream.truncate(32*1024*1024)
        with (temporary/'qemu.log').open('w') as output:
            process=subprocess.Popen([str(QEMU),'-machine','imx6sl-wario','-accel','qtest',
                '-m','512M','-display','none','-serial','none','-monitor','none',
                '-drive',f'file={disk},if=sd,index=1,format=raw',
                '-qtest',f'unix:{temporary}/qtest.sock,server=on,wait=off'],
                stdout=output,stderr=subprocess.STDOUT)
            try:
                with connect(temporary/'qtest.sock') as client:
                    client.settimeout(3)
                    stream=client.makefile('rwb',buffering=0)
                    def command(line):
                        stream.write(line.encode()+b'\n')
                        result=stream.readline().decode().strip()
                        assert result.startswith('OK'),result
                        return result.split(maxsplit=1)[1] if ' ' in result else ''
                    command('memset 0x82000000 0x10000 0xa5')
                    command('writel 0x2194004 0x00800200')
                    before=int(command('readl 0x2194004'),16)>>16
                    command('writel 0x2194000 0x82000000')
                    after=int(command('readl 0x2194004'),16)>>16
                    data=bytes.fromhex(command('read 0x82000000 0x10000').removeprefix('0x'))
                    report={'qemuSha256':hashlib.sha256(QEMU.read_bytes()).hexdigest(),
                            'blocksBeforeCommand':before,'blocksAfterAddressWrite':after,
                            'destinationUnchanged':data==bytes([0xa5])*65536}
                    command('writel 0x219402c 0x7')
                    command('writel 0x2194034 0xffffffff')
                    def issue(opcode,argument=0,flags=2):
                        command('writel 0x2194030 0xffffffff')
                        command(f'writel 0x2194008 {argument:#x}')
                        command(f'writel 0x219400c {(opcode<<24)|(flags<<16):#x}')
                        return int(command('readl 0x2194030'),16)
                    issue(0,flags=0)
                    issue(1,0x40300080)
                    issue(2,flags=1)
                    issue(3,0x20000)
                    issue(9,0x20000,flags=1)
                    issue(7,0x20000)
                    command('writel 0x2194004 0x00010200')
                    command('writel 0x2194000 0x82000000')
                    command('writel 0x2194048 0x13')
                    first=issue(8,flags=0x3a)
                    second=int(command('readl 0x2194030'),16)
                    remaining=int(command('readl 0x2194004'),16)>>16
                    extcsd=bytes.fromhex(command('read 0x82000000 0x200').removeprefix('0x'))
                    report.update(commandCompletionVisible=bool(first&1),
                                  dataCompletedWithoutAck=bool(second&2) and remaining==0,
                                  extcsdReceived=extcsd!=bytes([0xa5])*512)
                    report['passed']=(before==128 and after==128 and report['destinationUnchanged']
                        and report['commandCompletionVisible'] and report['dataCompletedWithoutAck']
                        and report['extcsdReceived'])
            finally:
                process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait()
        EVIDENCE.mkdir(exist_ok=True)
        (EVIDENCE/'emulated-storage-setup.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
        assert report['passed'],report


if __name__=='__main__':verify()
