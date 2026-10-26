# 构建

普通安装使用 [Release 预编译包](https://github.com/OceanJy333/kindle-hack/releases/latest)，见[设置指南](docs/setup.md)。以下为开发者构建，从仓库根目录运行。

界面采用 1072 × 1448 逻辑画布，按实际屏幕缩放。显示后端处理 8 / 16 bpp，触摸和 PagePress 按能力发现。安装器负责机型 profile 和运行探针；PW1、PW2、PW3、Voyage 的本轮安装包均待对应实机验证。`device.json` 保留逻辑布局配置，不是实机兼容证明。

## 完整 Release

需要 Node.js 24+、Python 3.12+、make、C/C++ 工具和已运行的 Docker。一条命令下载固定依赖、编译游戏与阅读器、执行安装器测试并打包：

```sh
sh tools/build-release.sh --version v0.2.0-ai-kit
```

输出 `build/release/kindle-hack-ai-kit.zip`、`kindle-hack-sources.tar.gz` 和 `SHA256SUMS`。已有完整构建产物时，可只组装发行文件：

```sh
python3 tools/package-release.py --version v0.2.0-ai-kit
```

发布为普通 GitHub Release，标签 `v0.2.0-ai-kit`；安装任务通过 `/releases/latest` 取得发行资产。

## 主机与游戏

需要 Python 3.12+、Node.js 24+、make、C/C++ 编译工具。下载脚本支持 macOS 和 Linux 的 arm64 / x86_64。

```sh
python3 tools/fetch-deps.py
npm ci --prefix native
node --test native/test.ts native/notifications.test.ts
sh native/build.sh
python3 doom/build.py
python3 arcade/build.py
```

依赖固定在 `tools/dependencies.json`。下载脚本校验 SHA-256；源码包、字体与工具链写到 `build/`。两个游戏构建会先从 TypeScript UI 生成覆盖层及 C 输入区域，再交叉编译 ARM 静态程序。

| 输出 | 内容 |
| --- | --- |
| `build/agent-board` / `build/agent-notify` | 主机桌面显示桥与通知客户端 |
| `build/doom/doom-voyage` | 静音 DOOM ARM 程序 |
| `build/doom/overlay.gray` | DOOM 控制界面 |
| `build/arcade/arcade-voyage` | 静音 Neo Geo ARM 前端 |
| `build/arcade/overlay.gray` | 街机控制界面 |

游戏数据留在设备的用户分区：DOOM 使用 `/mnt/us/games/doom1.wad`；Neo Geo 使用 `/mnt/us/games/roms/mslug.zip` 及同目录的 `neogeo.zip`。构建前端不下载或打包这些文件。

主机桌面入口：

```sh
BOARD_BIND=0.0.0.0 BOARD_CWD="$PWD" node native/host.ts
```

配对 token 首次运行写入 `native/runtime/device-token`。Agent 页读取本机 Codex SQLite / rollout 和 Claude 项目记录；其格式以当前适配源码为准。音乐控制调用主机的 `media-control` 命令。字体可通过 `DESK_FONT_DIR` 换成本机目录（需保留三种字重文件名）。

## 公共阅读器与 USB 包

按 [weread/BUILD.md](weread/BUILD.md) 构建 Qt/glibc 与客户端，再执行公共打包器：

```sh
python3 weread/official/package-public-runtime.py
```

输出 `build/weread/native-runtime/public-runtime/`。公共包使用自主 QML、Noto 字体和 CA 证书，无需官方客户端的 QML、图标或安装包。Qt 当前最低 Linux 3.0.0；PW1 的 Linux 2.6.31 不能运行该阅读器。

`installer/` 提供 USB 安装器、KUAL 扩展和设备探针。发行包应包含根目录 `install.py`、`extensions/ocean/` 和完整的预编译 payload；游戏 WAD、ROM、BIOS 由用户自备。固定命令与结果状态见[给 AI 的安装任务](docs/给AI的安装任务.md)。

## 旧私有运行包与独立系统

[weread/BUILD.md](weread/BUILD.md) 保留从用户自备官方包提取资源的私有镜像构建。这些资源留在本地 `build/`。独立内核与根文件系统按 [system/BUILD.md](system/BUILD.md) 单独构建，需要用户设备的硬件固件。

`native/device-start.sh`、`native/physical-desktop.sh` 和旧 ext2 镜像属于 Voyage 实验入口，包含旧电源交接；普通 USB 安装使用 `installer/extension/bin/launch.sh`，保留原厂 `powerd`。

## 验证范围

本次公开源码整理运行主机测试、UI 渲染和 ARM 交叉编译。新仓库未部署到真实 Kindle，独立系统镜像与 Qt 容器的全量构建结果以各自说明为准。
