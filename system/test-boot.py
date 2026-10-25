#!/usr/bin/env python3
# input: new kernel/rootfs and existing emulator; output: boot evidence; pos: disposable independent-system validation.
from pathlib import Path
from host_tools import E2FS
import gzip
import hashlib
import json
import shutil
import socket
import subprocess
import sys
import tempfile
import tarfile
import time
from boot_console import connect as connect_console, diagnose
from settings_test import verify_settings
from power_test import verify_power

SOURCE = Path(__file__).resolve().parent
REPO = SOURCE.parent
LAB = REPO / 'build'
WORK = LAB / 'independent-os'
EMULATOR = LAB / 'tools/eink-emulator'
EVIDENCE = SOURCE.parent / 'build/reports/independent-os'
sys.path.insert(0, str(EMULATOR))
from eink_emulator.images.imx6_mmc import build as build_disk


def test():
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    diagnostic='--diagnose-bootloader' in sys.argv
    kexec='--kexec' in sys.argv
    ramboot='--ramboot' in sys.argv or kexec
    settings='--settings' in sys.argv
    blocked_display='--blocked-display' in sys.argv
    power_failure='--power-failure' in sys.argv
    no_ack='--no-ack' in sys.argv
    bad_profile='--bad-wifi-profile' in sys.argv
    if no_ack and not kexec:raise ValueError('The unattended guard runs only on the RAM-boot path')
    if power_failure and blocked_display:raise ValueError('Power locking requires a working display')
    if blocked_display and settings:raise ValueError('Blocked display cannot run UI checks')
    bootloader='--bootloader' in sys.argv or diagnostic
    if ramboot and bootloader:raise ValueError('RAM boot uses a direct kernel entry')
    prefix='emulated-barebox-diagnostic' if diagnostic else 'emulated-barebox' if bootloader else 'emulated-independent'
    if ramboot:prefix='emulated-kexec' if kexec else 'emulated-ramboot'
    if settings:prefix+='-settings'
    if blocked_display:prefix+='-blocked-display'
    if power_failure:prefix+='-power-failure'
    if no_ack:prefix+='-no-ack'
    if bad_profile:prefix+='-bad-wifi-profile'
    tracing='--trace-storage' in sys.argv
    if tracing:prefix+='-traced'
    serial = EVIDENCE / (prefix+'-boot.log')
    with tempfile.TemporaryDirectory(prefix='boot-test-', dir=WORK) as folder:
        temporary = Path(folder)
        rootfs = temporary / 'rootfs.ext3'
        with gzip.open(WORK / 'voyage-rootfs.ext3.gz', 'rb') as source, rootfs.open('wb') as target:
            shutil.copyfileobj(source, target)
        if blocked_display:
            held=temporary/'held-display'
            held.write_text('#!/bin/sh\necho waiting >/run/ocean-display-held\nexec sleep 120\n')
            debugfs=str(E2FS / 'debugfs')
            for command in ['rm /usr/bin/ocean-desktop',f'write {held} /usr/bin/ocean-desktop',
                            'set_inode_field /usr/bin/ocean-desktop mode 0100755']:
                subprocess.run([debugfs,'-w','-R',command,rootfs],check=True,capture_output=True)
        userstore = temporary / 'userstore/ocean-install/ssh'
        userstore.mkdir(parents=True)
        access = LAB / 'device-install/physical-access'
        shutil.copyfile(access / 'id_ed25519.pub', userstore / 'authorized_keys')
        local = temporary / 'local.ext3'
        local_tree=temporary / 'local'
        hardware=local_tree / 'ocean/hardware'
        hardware.mkdir(parents=True)
        # This profile exists only in the disposable emulator disk.
        profile='ctrl_interface=/run/wpa_supplicant\nupdate_config=1\n'
        if bad_profile:profile='network={\n ssid=\nthis is not a valid profile\n'
        elif not settings:profile+='network={\n ssid="Kindle-QEMU"\n key_mgmt=NONE\n}\n'
        (local_tree/'ocean/wpa_supplicant.conf').write_text(profile)
        with tarfile.open(WORK / 'private-backup/hardware/firmware.tar.gz') as archive:
            archive.extractall(hardware,filter='data')
        # The real panel waveform exercises the display driver's parser; absent on a fresh checkout.
        panel=WORK / 'private-backup/hardware/panel.wrf.gz'
        if panel.exists():shutil.copyfile(panel,hardware/'panel.wrf.gz')
        with local.open('wb') as stream:
            stream.truncate(64 * 1024 * 1024)
        subprocess.run([str(E2FS / 'mke2fs'), '-q', '-F', '-t', 'ext3',
                        '-O', '^64bit,^metadata_csum', '-d', local_tree, local], check=True)
        disk = temporary / 'disk.raw'
        payload=temporary/'userstore/ocean-install'
        boot_root=rootfs
        if ramboot:
            os_files=temporary/'userstore/ocean-os';os_files.mkdir()
            shutil.copyfile(rootfs,os_files/'voyage-rootfs.ext3')
            shutil.copyfile(WORK/'artifacts/zImage',os_files/'zImage')
            shutil.copyfile(WORK/'ramboot/initramfs.cpio',os_files/'initramfs.cpio')
            boot_root=temporary/'empty-root.ext3'
            with boot_root.open('wb') as target:target.truncate(450*1024*1024)
            subprocess.run([str(E2FS / 'mke2fs'),'-q','-F','-t','ext3',
                            '-O','^64bit,^metadata_csum',boot_root],check=True)
            payload=[payload,os_files]
        build_disk(disk, WORK / 'artifacts/uImage', boot_root, layout='wario',local=local,userstore=payload)
        rootfs.unlink()
        local.unlink()
        command = [EMULATOR / 'qemu/build/qemu-system-arm', '-name', 'Ocean-independent-test',
                   '-machine', 'imx6sl-wario,idme-serial=B013000000000001', '-m', '512M',
                   '-kernel', WORK / 'artifacts/uImage',
                   '-append', 'root=/dev/mmcblk0p1 rootwait ro console=ttymxc0,115200 video=mxcepdcfb:E60,bpp=8,x_mem=3M loglevel=7',
                   '-drive', f'file={disk},if=sd,index=1,format=raw',
                   '-display', 'none', '-monitor', 'none', '-serial', 'file:' + str(serial), '-no-reboot',
                   '-qmp', f'unix:{temporary}/qmp.sock,server=on,wait=off',
                   '-netdev', 'user,id=usb,net=192.168.77.0/24,host=192.168.77.1,dhcpstart=192.168.77.20,hostfwd=tcp:127.0.0.1:19222-192.168.77.2:2222',
                   '-global', 'usb-chipidea.netdev=usb', '-global', 'usb-chipidea.mac=02:4f:43:45:41:01',
                   '-netdev', 'user,id=wifi', '-global', 'ar6003-sdio.netdev=wifi']
        if ramboot:
            command+=['-initrd',WORK/('ramboot/bootstrap.cpio' if kexec else 'ramboot/initramfs.cpio')]
            if kexec:command[command.index('-kernel')+1]=WORK/'ramboot/stock-uImage'
        if bootloader:
            offset=command.index('-kernel')
            command[offset:offset+4]=['-bios',WORK / 'bootloader/barebox-kindle-nm460gz.img']
        if diagnostic:
            offset=command.index('-serial')
            command[offset:offset+2]=['-chardev',f'socket,id=console,path={temporary}/serial.sock,server=on,wait=off,logfile={serial}', '-serial','chardev:console']
        if tracing:
            command+=['-trace',f'enable=sdcard_read_block,file={temporary}/storage.trace',
                      '-trace','enable=sdhci_send_command']
        output = (EVIDENCE / (prefix+'-qemu.log')).open('w')
        process = subprocess.Popen(list(map(str, command)), stdout=output, stderr=subprocess.STDOUT)
        console=connect_console(temporary/'serial.sock') if diagnostic else None
        report = {'kernel': '3.0.35-ocean-voyage', 'kernelReached': False,
                  'independentInitReached': False, 'sshAccepted': False, 'physicalBootValidated': False,
                  'bootloader':'barebox' if bootloader else 'stock-kernel-kexec' if kexec else 'kernel-initramfs' if ramboot else 'direct-kernel',
                  'qemuSha256':hashlib.file_digest((EMULATOR/'qemu/build/qemu-system-arm').open('rb'),'sha256').hexdigest(),
                  'kernelSha256':hashlib.file_digest((WORK/'artifacts/uImage').open('rb'),'sha256').hexdigest(),
                  'rootfsArchiveSha256':hashlib.file_digest((WORK/'voyage-rootfs.ext3.gz').open('rb'),'sha256').hexdigest()}
        if bootloader:report['bootloaderSha256']=hashlib.file_digest((WORK/'bootloader/barebox-kindle-nm460gz.img').open('rb'),'sha256').hexdigest()
        if ramboot:
            for name in ['initramfs.cpio']+(['bootstrap.cpio','stock-uImage'] if kexec else []):
                report[name+'Sha256']=hashlib.file_digest((WORK/'ramboot'/name).open('rb'),'sha256').hexdigest()
        ssh_command=['ssh', '-i', str(access / 'id_ed25519'), '-o', 'IdentitiesOnly=yes',
                     '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=accept-new',
                     '-o', 'UserKnownHostsFile=' + str(temporary / 'known_hosts'),
                     '-o', 'ConnectTimeout=1', '-p', '19222', 'root@127.0.0.1']
        started=time.monotonic()
        try:
            for attempt in range(60):
                if process.poll() is not None:
                    report['qemuExitCode'] = process.returncode
                    break
                if time.monotonic()-started>120:break
                time.sleep(2)
                log = serial.read_text(errors='replace') if serial.exists() else ''
                report['kernelReached'] = 'Linux version 3.0.35-ocean-voyage' in log
                report['independentInitReached'] = 'OCEAN_INIT_READY' in log
                report['bootloaderReached'] = 'barebox 2026.09.0' in log if bootloader else None
                report['usbConfigured']='g_ether gadget: high speed config #1' in log
                if ramboot:
                    report['ramRootReached']='OCEAN_RAMBOOT_ROOT_READY' in log
                    if kexec:report['kexecExecuted']='OCEAN_KEXEC_EXECUTING' in log
                if 'Kernel panic' in log or 'OCEAN_RAMBOOT_FAILED' in log:
                    report['bootFailure']='kernel-panic' if 'Kernel panic' in log else 'initramfs-failed'
                    break
                if 'Bad Data CRC' in log and 'Nothing bootable found' in log:
                    report['bootFailure']='bootloader-kernel-data-crc'
                    if diagnostic:
                        detail=diagnose(console,WORK/'artifacts/uImage',EVIDENCE/(prefix+'-reads.json'),temporary/'qmp.sock')
                        report['kernelReadRangesMatch']=[r['matches'] for r in detail['reads']]
                    break
                if bootloader and 'Nothing bootable found' in log:
                    report['bootFailure']='bootloader-no-bootable-entry'
                    break
                if attempt % 5 == 0:
                    print(json.dumps({'seconds': round(time.monotonic()-started), **report}), flush=True)
                # The bootloader exposes USB serial, not the Linux Ethernet service.
                if bootloader and not report['kernelReached']:continue
                if not report['usbConfigured']:continue
                desktop_check=('test -f /run/ocean-display-held; echo DISPLAY_HELD' if blocked_display else
                               'desktop_pid=$(cat /tmp/weread-login.pid); test -r /proc/$desktop_pid/status; head -4 /proc/$desktop_pid/status')
                ssh = subprocess.run(ssh_command+[
                                      'set -e; uname -r; readlink /proc/1/exe; cat /etc/os-release; cat /proc/mounts; ps | head -18; ' + desktop_check + '; set +e; /usr/sbin/wpa_cli -i wlan0 status; echo WIFI_STATUS_EXIT=$?'],
                                     capture_output=True, text=True, timeout=4)
                if ssh.returncode == 0:
                    report['sshAccepted'] = True
                    report['sshTransport']='usb-ethernet'
                    report['displayBlockedDuringBoot']=blocked_display and 'DISPLAY_HELD' in ssh.stdout
                    report['wifiControlReady']='WIFI_STATUS_EXIT=0' in ssh.stdout
                    if ramboot:report['loopRootMounted']=any(line.startswith('/dev/loop1 / ext3 ro,') for line in ssh.stdout.splitlines())
                    (EVIDENCE / (prefix+'-session.txt')).write_text(ssh.stdout)
                    time.sleep(5)
                    if settings:report['settings']=verify_settings(temporary/'qmp.sock',EVIDENCE,prefix,ssh_command)
                    wifi=subprocess.run(ssh_command+['for n in $(seq 1 12); do /usr/sbin/wpa_cli -p /run/wpa_supplicant -i wlan0 status; ifconfig wlan0; if ifconfig wlan0 | grep -q "inet addr:10.0.2."; then break; fi; sleep 1; done; if ifconfig wlan0 | grep -q "inet addr:10.0.2."; then ping -I wlan0 -c 1 -W 2 10.0.2.2; echo GATEWAY_PING_EXIT=$?; fi; dmesg | grep OCEAN_BOOT; cat /run/ocean-wifi-dhcp.log; cat /proc/net/route; dmesg | tail -25'],
                                        capture_output=True,text=True,timeout=20)
                    (EVIDENCE/(prefix+'-wifi.txt')).write_text(wifi.stdout+wifi.stderr)
                    report['bootStagesRecoverable']=all(('OCEAN_BOOT '+stage) in wifi.stdout for stage in
                        ['storage-ready','ocean-usb-prepared','ocean-ssh-prepared','ocean-wifi-prepared'])
                    report['wifiControlReady']='wpa_state=' in wifi.stdout
                    if bad_profile:
                        kept=subprocess.run(ssh_command+['test -s /var/local/ocean/wpa_supplicant.conf.unreadable && grep -q "not a valid" /var/local/ocean/wpa_supplicant.conf.unreadable && grep -q ctrl_interface /var/local/ocean/wpa_supplicant.conf'],
                                            capture_output=True,text=True,timeout=8)
                        report['badProfileKeptAndReplaced']=kept.returncode==0
                    report['wifiAssociated']='wpa_state=COMPLETED' in wifi.stdout
                    report['wifiDhcpReady']='inet addr:10.0.2.' in wifi.stdout
                    report['wifiGatewayReachable']=report['wifiDhcpReady'] and 'GATEWAY_PING_EXIT=0' in wifi.stdout
                    if kexec:
                        for _ in range(10):
                            box=subprocess.run(ssh_command+[
                                'cd /mnt/us/ocean-os/blackbox/current; cat events.log; echo ==; grep -c . dmesg.log; '
                                'echo ==; grep -c . ../early-dmesg.log; echo ==; ls; echo ==; cat snapshot-2.txt'],
                                capture_output=True,text=True,timeout=8)
                            if box.returncode==0 and 'snapshot-2.txt' in box.stdout:break
                            time.sleep(3)
                        (EVIDENCE/(prefix+'-blackbox.txt')).write_text(box.stdout+box.stderr)
                        report['blackboxWritten']=box.returncode==0 and '0 boot 3.0.35-ocean-voyage' in box.stdout and '== modules' in box.stdout
                    if kexec and not no_ack:
                        acknowledgement=subprocess.run(ssh_command+[
                            'set -e; test "$(cat /sys/module/kernel/parameters/ocean_boot_timeout)" = 180; '
                            'echo 0 >/sys/module/kernel/parameters/ocean_boot_timeout; '
                            'test "$(cat /sys/module/kernel/parameters/ocean_boot_timeout)" = 0; echo BOOT_ACCEPTED'],
                            capture_output=True,text=True,timeout=5)
                        report['bootGuardAcknowledged']=acknowledgement.returncode==0 and 'BOOT_ACCEPTED' in acknowledgement.stdout
                    if no_ack:
                        # Nobody acknowledges: the userspace guard must reboot, ending the emulator.
                        try:process.wait(timeout=220);report['qemuExitCode']=process.returncode
                        except subprocess.TimeoutExpired:pass
                        final=serial.read_text(errors='replace')
                        report['guardRebooted']=process.poll() is not None and 'OCEAN_BOOT ocean-guard-expired' in final
                        break
                    if power_failure:report['power']=verify_power(temporary/'qmp.sock',EVIDENCE,prefix,ssh_command,serial)
                    if blocked_display:break
                    with socket.socket(socket.AF_UNIX) as client:
                        client.settimeout(5)
                        client.connect(str(temporary / 'qmp.sock'))
                        stream=client.makefile('rwb',buffering=0)
                        stream.readline()
                        for action in [{'execute':'qmp_capabilities'},
                                       {'execute':'screendump','arguments':{'filename':str(EVIDENCE / (prefix+'-home.png')),'format':'png'}}]:
                            stream.write(json.dumps(action).encode()+b'\n')
                            while True:
                                response=json.loads(stream.readline())
                                if 'error' in response:raise RuntimeError(response)
                                if 'return' in response:break
                    break
                (EVIDENCE / (prefix+'-ssh-error.txt')).write_text(ssh.stderr)
                if 'Kernel panic' in log:
                    report['kernelPanic'] = True
                    break
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            output.close()
            if console:console.close()
            if tracing and (temporary/'storage.trace').exists():
                shutil.copyfile(temporary/'storage.trace',EVIDENCE/(prefix+'-storage.trace'))
        (EVIDENCE / (prefix+'-boot.json')).write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
        if not all(report[name] for name in ['kernelReached','independentInitReached','sshAccepted','bootStagesRecoverable']):
            raise SystemExit(1)
        if ramboot and not all(report.get(name) for name in ['ramRootReached','loopRootMounted']+(['kexecExecuted'] if kexec else [])):
            raise SystemExit(1)
        if blocked_display and not all(report.get(name) for name in
            ['displayBlockedDuringBoot','wifiAssociated','wifiDhcpReady','wifiGatewayReachable']):raise SystemExit(1)
        if kexec and not no_ack and not report.get('bootGuardAcknowledged'):raise SystemExit(1)
        if kexec and not report.get('blackboxWritten'):raise SystemExit(1)
        if bad_profile and not (report.get('badProfileKeptAndReplaced') and report.get('wifiControlReady')):raise SystemExit(1)
        if no_ack and not report.get('guardRebooted'):raise SystemExit(1)
        if power_failure and (not report.get('power') or not all(report['power'].values())):raise SystemExit(1)
        if settings and not all(report.get('settings',{}).get(name) is True for name in
            ['touchJoinCompleted','profileSaved','singleProfile','keyboardTouchPassed']):raise SystemExit(1)


if __name__ == '__main__':
    test()
