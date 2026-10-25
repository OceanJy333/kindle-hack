# 构建

从仓库根目录运行。目标设备配置在 `device.json`，默认 Kindle Voyage（1072 × 1448）。源码按这台设备的 framebuffer、触摸、PagePress 与电源接口适配。

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

## 微信读书与自编系统

先按 [weread/BUILD.md](weread/BUILD.md) 构建原生 Qt 运行时，再按 [system/BUILD.md](system/BUILD.md) 构建独立内核与根文件系统。官方资源从使用者自备的安装包本地提取，留在 `build/`。独立系统构建还需要从自有设备提取的硬件固件。

`native/physical-bootstrap.sh`、`native/physical-desktop.sh` 和 `weread/official/port/launch.sh` 是设备侧入口；[配置说明](docs/setup.md) 说明它们的安装位置。先完成越狱和设备信息核对，再由自己的 AI 根据设备配置处理部署。

## 验证范围

本次公开源码整理运行主机测试、UI 渲染和 ARM 交叉编译。新仓库未部署到真实 Kindle，独立系统镜像与 Qt 容器的全量构建结果以各自说明为准。
