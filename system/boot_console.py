#!/usr/bin/env python3
# input: private emulator serial socket; output: bounded read diagnostics; pos: bootloader I/O investigation.
import hashlib
import json
import socket
import struct
import time


def connect(path):
    client=socket.socket(socket.AF_UNIX)
    deadline=time.monotonic()+3
    while True:
        try:
            client.connect(str(path));break
        except (FileNotFoundError,ConnectionRefusedError):
            if time.monotonic()>deadline:raise
            time.sleep(0.05)
    return client


def command(client,line,timeout=8):
    client.sendall((line+'; echo __OCEAN_DONE__\n').encode())
    result=bytearray();deadline=time.monotonic()+timeout
    client.settimeout(0.2)
    while time.monotonic()<deadline:
        try:
            data=client.recv(65536)
            if not data:break
            result.extend(data)
        except socket.timeout:continue
        marked=any(part.strip()==b'__OCEAN_DONE__' for part in result.splitlines())
        # barebox flushes pending input before printing the next prompt.
        if marked and b'barebox@' in result[result.rfind(b'__OCEAN_DONE__'):]:break
    return result.decode(errors='replace')


def diagnose(client,image,destination,qmp_path=None):
    data=image.read_bytes();size=struct.unpack_from('>I',data,12)[0]
    ranges=[(0,64),(57344,4096),(61440,4096),(65536,4096),
            (64,57344),(64,61440),(64,65536),(64,size)]
    report={'reads':[]}
    for offset,length in ranges:
        expected=hashlib.md5(data[offset:offset+length]).hexdigest()
        output=command(client,f'md5sum /dev/mmc1.kernel {offset:#x}+{length:#x}')
        report['reads'].append({'offset':offset,'bytes':length,'expectedMd5':expected,
                                'matches':expected in output,'output':output})
    report['payloadStart']=command(client,'md -s /dev/mmc1.kernel -b 0x40+0x80')
    report['expectedPayloadStart']=data[64:192].hex()
    report['controller']=command(client,'md -l 0x2194000+0x50')
    if qmp_path:
        report['copy']=command(client,f'memcpy -s /dev/mmc1.kernel 0 0x82000000 {len(data):#x}')
        capture=destination.with_suffix('.bin')
        with socket.socket(socket.AF_UNIX) as monitor:
            monitor.connect(str(qmp_path));monitor.settimeout(5)
            stream=monitor.makefile('rwb',buffering=0);stream.readline()
            for action in [{'execute':'qmp_capabilities'}, {'execute':'human-monitor-command',
                           'arguments':{'command-line':f'pmemsave 0x82000000 {len(data)} "{capture}"'}}]:
                stream.write(json.dumps(action).encode()+b'\n')
                while True:
                    response=json.loads(stream.readline())
                    if 'error' in response:raise RuntimeError(response)
                    if 'return' in response:break
        actual=capture.read_bytes()
        differences=[i for i,(a,b) in enumerate(zip(actual,data)) if a!=b]
        report['capturedRead']={'bytes':len(actual),'matches':actual==data,
                                'differenceCount':len(differences),'firstDifferences':differences[:32]}
    report['retryBoot']=command(client,'bootm -c /dev/mmc1.kernel',timeout=8)
    destination.write_text(json.dumps(report,indent=2)+'\n')
    return report
