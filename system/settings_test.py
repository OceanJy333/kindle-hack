#!/usr/bin/env python3
# input: disposable guest QMP and SSH; output: native touch/network evidence; pos: first-run settings acceptance.
import json
import socket
import subprocess
import time
from pathlib import Path


class Screen:
    def __init__(self,path):
        self.client=socket.socket(socket.AF_UNIX)
        self.client.settimeout(5);self.client.connect(str(path))
        self.stream=self.client.makefile('rwb',buffering=0);self.stream.readline()
        self.send('qmp_capabilities')

    def send(self,execute,arguments=None):
        self.stream.write(json.dumps({'execute':execute,'arguments':arguments or {}}).encode()+b'\n')
        while True:
            result=json.loads(self.stream.readline())
            if 'error' in result:raise RuntimeError(result)
            if 'return' in result:return result['return']

    def tap(self,x,y):
        self.send('input-send-event',{'events':[
            {'type':'abs','data':{'axis':'x','value':round(x*32767/1071)}},
            {'type':'abs','data':{'axis':'y','value':round(y*32767/1447)}},
            {'type':'btn','data':{'button':'left','down':True}}]})
        time.sleep(0.18)
        self.send('input-send-event',{'events':[{'type':'btn','data':{'button':'left','down':False}}]})

    def capture(self,path):
        self.send('screendump',{'filename':str(path),'format':'png'})

    def close(self):self.stream.close();self.client.close()


def verify_settings(qmp_path,evidence,prefix,ssh):
    screen=Screen(qmp_path)
    try:
        screen.tap(950,182);time.sleep(4)
        screen.capture(evidence/(prefix+'-networks.png'))
        screen.tap(920,590);time.sleep(1)
        screen.capture(evidence/(prefix+'-join.png'))
        screen.tap(166,570);time.sleep(5)
        screen.capture(evidence/(prefix+'-connected.png'))
        # Rejoining the same SSID must replace its profile, not accumulate it.
        screen.tap(920,590);time.sleep(0.5)
        screen.tap(166,570);time.sleep(4)
        result=subprocess.run(ssh+['/usr/sbin/wpa_cli -i wlan0 status; /usr/sbin/wpa_cli -i wlan0 list_networks; cat /var/local/ocean/wpa_supplicant.conf'],capture_output=True,text=True,timeout=5)
        (evidence/(prefix+'-settings.txt')).write_text(result.stdout+result.stderr)
        report={'touchJoinCompleted':'wpa_state=COMPLETED' in result.stdout,
                'profileSaved':'ssid="Kindle-QEMU"' in result.stdout,
                'singleProfile':result.stdout.count('network={')==1}
        tool=Path(__file__).resolve().parents[1]/'build/weread/native-runtime/settings-preview'
        upload=subprocess.run(ssh+['cat >/tmp/ocean-settings-preview; chmod 700 /tmp/ocean-settings-preview'],input=tool.read_bytes(),capture_output=True,timeout=8)
        assert upload.returncode==0,upload.stderr
        preview=subprocess.run(ssh+['QT_QPA_PLATFORM=offscreen QSG_RENDER_LOOP=basic QT_QUICK_BACKEND=software QT_PLUGIN_PATH=/opt/ocean/plugins FONTCONFIG_FILE=/opt/ocean/fonts.conf XDG_RUNTIME_DIR=/var/local/weread-port/runtime /opt/ocean/ld-linux-armhf.so.3 --library-path /opt/ocean /tmp/ocean-settings-preview /opt/ocean /tmp/ocean-keyboard.png'],capture_output=True,text=True,timeout=10)
        (evidence/(prefix+'-keyboard.txt')).write_text(preview.stdout+preview.stderr)
        report['keyboardTouchPassed']=preview.returncode==0 and '"keyboardTouchPassed":true' in preview.stdout
        if '"renderSaved":true' in preview.stdout:
            pixels=subprocess.run(ssh+['cat /tmp/ocean-keyboard.png'],capture_output=True,timeout=5)
            assert pixels.returncode==0 and pixels.stdout.startswith(b'\x89PNG'),pixels.stderr
            (evidence/(prefix+'-keyboard.png')).write_bytes(pixels.stdout)
        screen.tap(900,1180);time.sleep(1)
        sample=subprocess.run(ssh+['p=$(cat /tmp/weread-login.pid); cat /proc/$p/stat; sleep 5; cat /proc/$p/stat; grep VmRSS /proc/$p/status'],
                              capture_output=True,text=True,timeout=8)
        (evidence/(prefix+'-idle.txt')).write_text(sample.stdout+sample.stderr)
        stats=sample.stdout.splitlines()
        if len(stats)>=2:
            before=stats[0].split(') ',1)[1].split();after=stats[1].split(') ',1)[1].split()
            report['guiIdleCpuTicksOver5s']=sum(map(int,after[11:13]))-sum(map(int,before[11:13]))
        return report
    finally:screen.close()
