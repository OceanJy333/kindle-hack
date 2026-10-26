# 微信读书 ARMv7 构建

本目录保存登录、书架、阅读、离线缓存与设备适配源码。公共阅读器使用自主 QML 界面，随包组装 Qt、Noto 字体和 CA 证书。普通用户安装 [Release 预编译包](https://github.com/OceanJy333/kindle-hack/releases/latest)；以下命令供开发者从仓库根目录执行。

公共包按第 2、3 步构建，再执行“公共运行目录”打包。第 1 步和 ext2 镜像属于保留的私有实验入口，不是公共包的前置条件。

## 固定输入

| 输入 | 固定版本与来源 |
| --- | --- |
| Debian 容器 | `bookworm-20260202-slim`，镜像索引 SHA-256 `98f4b71de414932439ac6ac690d7060df1f27161073c5036a7553723881bffbe` |
| ARMhf Qt 与系统库 | Debian `20260201T000000Z` 快照，Bookworm Qt 6.4.2；Dockerfile 同时固定主仓库和 security 仓库 |
| glibc | GNU 2.39，SHA-256 `f77bd47cf8170c57365ae7bf86696c118adb3b120d3259c64c502d3dc1e2d926`；下载地址写在 `official/prepare-runtime.sh` |
| 字体与静态工具链 | 根目录 `tools/dependencies.json`：Noto Sans CJK 2.004、Zig 0.15.2；运行 `python3 tools/fetch-deps.py` |
| 仅旧私有包：官方客户端输入 | 用户自备 RemarkableWeRead 1.0.0 安装包；其 `payload/bin/remarkable-weread` 的 SHA-256 必须为 `5a117d08c8503bad49b65a54357cdc1698b0ae3e52550d704fe9c6262f061ac7` |

官方入口是 [微信读书墨水屏版](https://ink.qq.com/)，对应包为 [reMarkable 1.0.0 ZIP](https://cdn.weread.qq.com/app/remarkable/remarkable-weread-v1.0.0-universal-release.zip)。自行下载并解开 ZIP 和内层 tar.gz，保留原始 ELF。后续工具核对该 ELF 的哈希。

## 1. 旧私有包：提取自己的官方界面资源

设置 `WEREAD_ELF` 为已解包文件的绝对路径。提取器读取 ELF 中的四组 Qt RCC 数据表，不启动客户端。

```sh
WEREAD_ELF=/absolute/path/to/payload/bin/remarkable-weread
mkdir -p build/weread
python3 weread/official/recover-resources.py "$WEREAD_ELF"
```

需要 Python 3.11+ 和 libzstd。macOS 可用 `brew install zstd`；下述 Docker 工具链也包含 libzstd。默认输出 `build/weread/official-recovered-resources/` 和 `build/weread/resource-hashes.json`。恢复出来的资源留在本地 `build/`。

## 2. 获取 Qt、编译器和 glibc 源码

Docker 需已运行；工具链使用本机架构，交叉编译目标是 ARMv7 hard-float。

```sh
python3 tools/fetch-deps.py
docker build -t kindle-hack-weread \
  -f weread/official/native-build.Dockerfile weread/official
mkdir -p build/weread/native-runtime
docker run --rm \
  -v "$PWD:/src:ro" -v "$PWD/build/weread:/lab" \
  kindle-hack-weread sh /src/weread/official/prepare-runtime.sh
```

此步按快照依赖关系获取 ARMhf `.deb`，解包到 `build/weread/native-runtime/debian-root/`；Qt 头文件另放 `qt-headers/`。所有 `.deb` 的实际 SHA-256 记入 `packages.sha256`。glibc 归档下载后核对固定 SHA-256。

## 3. 编译运行库与客户端

```sh
docker run --rm \
  -v "$PWD:/src:ro" -v "$PWD/build/weread:/lab" \
  kindle-hack-weread sh /src/weread/official/build-native-libc.sh
docker run --rm \
  -v "$PWD:/src:ro" -v "$PWD/build/weread:/lab" \
  kindle-hack-weread sh /src/weread/official/port/build.sh
```

glibc 保留两项原有补丁：把 ARM 最低内核设为 Linux 3.0.0；修复缺少 `getrandom` 时的 `ENOSYS` 判断。客户端直接编译根目录 `native/display.c`，不再依赖临时复制的显示源码。

输出在 `build/weread/native-runtime/`：`glibc-install/`、`weread-login`、`wifi-data-check`、`settings-preview`、`runtime-check`。生成的 moc 文件和对象放在其中的 `port/`，源码目录保持干净。

可单独编译阅读位置回归程序：

```sh
docker run --rm \
  -v "$PWD:/src:ro" -v "$PWD/build/weread:/lab" \
  kindle-hack-weread sh /src/weread/official/port/build-progress-test.sh
```

## 4. 公共运行目录

完成第 2、3 步后，在宿主运行：

```sh
python3 weread/official/package-public-runtime.py
```

输出 `build/weread/native-runtime/public-runtime/`，包含自主 QML、ARMhf 库、补丁 glibc、Qt 插件、字体和 CA。`runtime-manifest.json` 记录依赖与哈希；`privateAssetsIncluded` 为 `false`。该目录无需 `original/` 或官方资源提取目录。

公共 UI 使用 1072 × 1448 逻辑画布，按 framebuffer 尺寸缩放；物理显示与输入由共用后端处理。构建目标为 ARMv7-A / VFPv3-D16 hard-float；glibc 最低 Linux 3.0.0。PW1 的 Linux 2.6.31 由安装器报告 `reader_runtime_unavailable`，不调用 Qt loader。

公共 USB 包将此目录放到 `extensions/ocean/payload/runtime/`，由 KUAL 的 `installer/extension/bin/launch.sh` 启动。它保留原厂 `powerd`，账号写 `ocean/state/weread/`，离线缓存写 `weread-offline/`。手动休眠结束会话并回原厂，行为待实机验证。

## 5. 旧私有包：组装本地镜像

安装 `mke2fs`：Debian/Ubuntu 用 `e2fsprogs`，macOS 用 `brew install e2fsprogs`。在宿主执行打包，使用已下载的宿主 Zig 编译电源服务。

```sh
# Linux
python3 weread/official/package-native-runtime.py
# macOS
python3 weread/official/package-native-runtime.py \
  --mke2fs "$(brew --prefix e2fsprogs)/sbin/mke2fs"
```

打包器复制 ARMhf 库、补丁 glibc、自主界面、Noto 字体、CA 根证书和用户本地提取的官方资源。`patch_qml.py` 在暂存目录追加 Voyage 翻页、笔记、阅读位置和离线提示。原始提取目录不变。登录会话、Cookie 和缓存从设备状态目录生成，不从电脑打包。

最终文件为 `build/weread/native-runtime/native-runtime.ext2.gz`；`package.json` 记录库与成品哈希。镜像解压为 192 MiB。此镜像包含自行提取的官方资源，应保留在本地。

若已编译根目录游戏，可加 `--with-games` 带上游戏程序和自主覆盖图。ROM、BIOS 和 WAD 不进入此打包步骤。

## 旧私有包与独立系统启动入口

自主系统使用 [system/BUILD.md](../system/BUILD.md) 的 rootfs 与启动流程，把本镜像内容并入系统。桌面启动入口是 `native/device-start.sh`；微信读书程序入口是镜像内 `launch.sh`。

在已有 root shell 的实验系统上，可将镜像解压、loop 挂载到 `/tmp/weread-native`，由当前系统的显示和输入交接流程运行 `/tmp/weread-native/launch.sh`。程序使用 `/dev/fb0`、evdev 和 Voyage EPDC 接口；状态默认写入 `/var/local/weread-port`，离线书籍写入 `/mnt/us/weread-offline`。

公共原厂安装走 `installer/`，见[设置指南](../docs/setup.md)。上面的旧 `native/device-start.sh` 会接管 Voyage 电源与桌面，普通安装不要使用。PW2、PW3、Voyage 的公共包为预适配、待各型号实机验证；显示、实体按键、休眠、续航和网络登录按实测结果分别记录。

## 源码整理时的检查记录

- 使用原 1.0.0 ELF 在临时目录提取了 33 个资源；`patch_reader` 完整通过。临时资源未复制进仓库。
- Python 语法与 shell 语法检查通过。
- 已核对 Debian 镜像 tag 和 digest，以及 GNU glibc 2.39 归档哈希。
- 上述旧资源整理检查未覆盖公共包的完整 Qt/glibc 构建、安装和设备启动；本轮 Release 的构建与校验结果以 Release 说明为准。
