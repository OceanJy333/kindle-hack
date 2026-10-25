# 设置 Kindle

## 准备

确认 Kindle 的准确型号和固件。原厂系统可在「设置 → 设备选项 → 设备信息」查看；连接成功后补查 `uname -a`、`uname -m` 和系统挂载。电脑需要 Git，以及 [BUILD.md](../BUILD.md) 中目标模块的工具。

**先完成越狱。** [KindleModding 越狱向导](https://kindlemodding.org/jailbreak-wizard.html) 按序列号前 8 位、固件版本、注册与广告状态选择方法。需要识别机型时提供序列号前缀即可。

2026-10-26 查阅的 [WinterBreak 项目指南](https://kindlemodding.org/jailbreaking/WinterBreak/)要求已注册的受支持机型、固件和 Wi-Fi，并明确不支持 `5.18.1` 及之后的固件。具体方法按当前向导结果执行。按所选指南完成越狱后的软件与更新管理，再准备 root 启动入口和 SSH。

## 按当前系统选择入口

| 当前状态 | 下一步 |
| --- | --- |
| 原厂系统，未越狱 | 按机型与固件完成越狱向导，再回来安装应用 |
| 原厂系统，已越狱 | 接通 root SSH；构建原生运行包；先手动启动所选功能 |
| 已运行 Ocean Voyage 独立系统 | 检查内核、`/opt/ocean`、`/tmp/weread-native` 和现有服务；按该布局更新对应程序 |
| 其他 Kindle 型号 | 先核对 CPU、显示 ioctl、触摸设备、分辨率、库和固件；据此移植 |
| 想自行换系统 | 单独阅读 `system/` 构建与启动代码，确定设备专用镜像、面板数据和恢复入口后制定刷写任务 |

## 建立连接

使用自己设备上已启用的 SSH。电脑生成自己的密钥，只将公钥放到 Kindle。首次连接核对设备显示或现场取得的主机指纹。

```sh
ssh-keygen -t ed25519 -f ~/.ssh/kindle
ssh -i ~/.ssh/kindle -p <端口> root@<Kindle地址>
```

尖括号内容替换为实际值。端口、USB 网络地址和 Wi-Fi 地址以自己的 SSH 设置为准。把它们填入仓库连接脚本支持的参数，或直接用 `ssh` / `scp`。

原厂越狱系统也保留了 `native/physical-bootstrap.sh` 的 Ocean Connect 入口。它由现有 root 启动器执行，读取以下设备文件：

| 设备路径 | 内容 |
| --- | --- |
| `/mnt/us/ocean-install/ssh/dropbear`、`dropbearkey` | 能在该原厂系统直接执行的 ARM SSH 程序 |
| `/mnt/us/ocean-install/ssh/authorized_keys` | 自己电脑的公钥 |
| `/mnt/us/ocean-install/board-host` | 允许连接的电脑局域网 IPv4 地址 |

运行后 SSH 监听 2222，设备信息写到 `/mnt/us/ocean-install/physical-inventory.txt`。独立系统的 `system/build-ssh.py` 产物使用新系统运行库；原厂系统须先核对动态加载器和依赖。

连接后先读取这些信息：

```sh
uname -a
uname -m
cat /proc/mounts
cat /proc/bus/input/devices
ls /sys/class/graphics/
ls /sys/class/power_supply/
```

Voyage 的起始适配为 1072 × 1448、ARMv7。自编内核版本为 `3.0.35-ocean-voyage`。内核版本与程序安装位置一起决定启动方式。

## 构建并安装需要的功能

依 [BUILD.md](../BUILD.md) 准备外部输入并构建。只构建用户选择的模块。先检查每个启动脚本读写的路径，再复制产物。

| 目标 | 源码入口与数据位置 |
| --- | --- |
| 原生桌面 | `native/`；原厂系统启动脚本为 `native/device-start.sh` 和 `native/physical-desktop.sh` |
| 微信读书 | `weread/official/port/`；运行包由 `weread/official/package-native-runtime.py` 生成 |
| DOOM | `doom/build.py`、`doom/launch.sh`；需已安装阅读器运行包，游戏数据由使用者提供 |
| 合金弹头 | `arcade/build.py`；资源放设备 `/mnt/us/games/roms/`，需匹配 FBA v0.2.97.29 |
| 共用游戏启动与刷新 | `games/launch.sh`、`games/native-launch.sh`、`games/display-worker.c` |
| 独立系统 | `system/`；根文件系统内的应用位于 `/opt/ocean` |

原厂系统的旧布局将 `native-runtime.ext2` 放到 `/mnt/us/`，把 `native/device-start.sh` 放到 `/mnt/us/ocean-install/device-start.sh`。现有 root 启动器可执行 `native/physical-desktop.sh` 打开桌面；它读取同目录的 `board-host`。`device-start.sh` 挂载运行包到 `/var/tmp/weread-native`，建立 `/tmp/weread-native` 链接，并停止原厂桌面与电源服务后启动本项目服务。

独立系统的 `/tmp/weread-native` 通常指向 `/opt/ocean`。让 AI 检查现有挂载和启动脚本，选择对应布局。

先手动运行，验证后再添加本机支持的启动入口。确认入口从重启后的系统也能打开，再记为安装完成。

## 验证

- **微信读书**：设备扫码登录；打开书籍、翻页；查段落评论；手机与 Kindle 各改一次阅读位置；下载自己可读的章节后断网再打开。
- **游戏**：开始、移动、射击、组合按键、退出返回；实看运动清晰度与残影。供帧频率、驱动完成次数和屏幕可见帧率分别记录。
- **桌面**：触摸、亮度、电量、休眠与唤醒；如接电脑看板，再查任务状态回传。

扫码登录、实体按键、屏幕观感和断电 / 重启体验需要用户在设备旁配合。AI 可完成本地构建、文件传输、配置和日志检查。

若微信读书 HTTPS 连接失败，检查设备时钟、运行包中的 `ca-certificates.crt`，以及 `weread/official/port/main.cpp` 是否实际加载它。当前源码含修复，需重新构建并确认设备加载的是新版。`system/shoot-overlay.sh` 的 `/tmp` 覆盖重启后消失；日常安装应将 CA 加载修复编译进实际启动的运行包或系统产物。
