# 自编系统构建

从仓库根目录运行。需要 Python 3.12+、Docker、主机 `make`、Zig 0.15.2 和 e2fsprogs。macOS 可用 `brew install e2fsprogs`；Linux 安装发行版的 e2fsprogs。主机通过 `PATH` 查找 `debugfs`，也可设置 `E2FS_DIR`。Docker 沿用当前 context 和外部 `DOCKER_HOST`。

## 取源码与启动构建容器

```sh
python3 tools/fetch-deps.py             # 包含 Zig，写到 build/native-deps/zig
python3 system/fetch-sources.py --list  # 查看版本、URL、SHA-256
python3 system/fetch-sources.py         # 下载并校验六项源码
mkdir -p build/independent-os/artifacts
docker build -t kindle-system-build -f system/Dockerfile .
docker run -d --name voyage-os-build \
  -v "$PWD/build/independent-os:/work" kindle-system-build
```

容器名是 `voyage-os-build`，主机 `build/independent-os` 与容器 `/work` 对应。容器工具链固定 Debian Bookworm 基础镜像 digest 和 `20260201T000000Z` apt snapshot，与原生 Qt 构建环境使用同一基线。六项源码的固定版本与 SHA-256 在 `sources.py`，下载不会跟随最新版本。

| 输入 | 获取方式 |
| --- | --- |
| Linux 3.0.35 Lab126 | `katadelos/linux-3.0.35-lab126`，提交 `7ac2585f384573da8cbdcc987a3503e2e6ff1fb4` |
| BusyBox 1.36.1 | 上游 release archive；配置在 `configs/busybox.config` |
| barebox 2026.09.0 | 上游 release archive，NM460GZ 配置 |
| Dropbear 2026.94 | 上游发布指定的 `dropbear.nl` mirror |
| wpa_supplicant 2.12 / libnl 3.12.0 | 上游 release archives |
| Qt、glibc、OpenSSL sysroot | 按 [weread/BUILD.md](../weread/BUILD.md) 构建 |

## 编译公开组件

```sh
python3 system/build-kernel.py
python3 system/build-busybox.py
python3 system/build-ssh.py
python3 system/build-wifi.py
python3 system/build-power.py
python3 system/build-power.py --test  # 可选：在容器执行 ARM 电源回归
python3 system/build-rootfs.py
```

内核采用 `configs/kernel.config`。GCC 兼容性和 Voyage 修改通过 `patches/0001`、`0004`–`0007` 应用，不需要私有源码覆盖目录。内核输出位于 `build/independent-os/artifacts`；根文件系统输出为 `build/independent-os/voyage-rootfs.ext3.gz`。

`build-rootfs.py` 还需要原生运行时生成的 `native-runtime.ext2.gz`、`glibc-install/lib` 和 `wifi-data-check`。`build-wifi.py` 需要同目录 `debian-root/usr` 的 ARMhf OpenSSL 头文件及 `libssl.so.3`、`libcrypto.so.3`。这些由微信读书构建步骤提供。

原厂系统使用静态 soft-float Dropbear：

```sh
python3 system/build-stock-ssh.py
```

输出 `build/stock-ssh/dropbear` 和 `dropbearkey`，使用 Zig 0.15.2、`arm-linux-musleabi`、`cortex_a9`。`ZIG` 可指定工具链可执行文件。`build-ssh.py` 的 ARMhf 动态版本用于独立系统。

## RAM 启动与自有设备输入

以下脚本需要使用者自有设备的输入：

| 输入路径（均在 build/independent-os） | 用途 |
| --- | --- |
| `private-backup/runtime-kernel.config`、`runtime-kallsyms.txt` | `build-kexec.py` 匹配原厂内核配置与导出符号 |
| `private-backup/disk-prefix.bin` | `build-ramboot.py` 从偏移 `0x41000` 读取原厂 uImage |
| `private-backup/hardware/{firmware.tar.gz,panel.wrf.gz}` | 自有面板、Wi-Fi、触摸固件；由 `capture-hardware.py` 获取 |

仓库不包含这些输入。内核模块还需要先生成的 `artifacts/Module.symvers`。准备后依次运行 `build-kexec.py`、`build-ramboot.py`。`capture-hardware.py`、`device.sh`、`stage-ramboot.py` 和 recovery 脚本会操作设备，按实际用途单独调用。

`device.sh` 默认 USB 地址 `192.168.77.2:2222`；Wi-Fi 设置 `KINDLE_HOST`。密钥默认放在 `build/device-install/physical-access`，可用 `KINDLE_ACCESS_DIR`、`KINDLE_KEY` 覆盖。原厂 recovery 的 `allowed-peers` 文件每行填写允许连接 SSH 的主机 IP；默认允许 USB 主机 `192.168.77.1`。

## 模拟器与验证

`test-boot.py`、`test-boot-deadline.py`、`verify-storage.py` 依赖自定义 `imx6sl-wario` QEMU 与 `eink_emulator` Python 包，路径是 `build/tools/eink-emulator`。QEMU 补丁 `0002` 对应基线提交 `b1a5347b628f13afe88454a096b5c3c78d84da5c`；完整模拟器仓库来源与获取流程尚未整理，普通上游 QEMU 不能直接运行这些测试。`0003` 是其多目录 userstore 补丁。

验证报告写入 `build/reports/independent-os`。新仓库验证范围包括 Python/shell 语法、newc 归档 round trip、firmware CRC、内核补丁适用性、电源组件 ARM 交叉编译，以及原厂 Dropbear 的静态 soft-float ELF 编译检查；未在新目录全量构建系统镜像或启动实机。
