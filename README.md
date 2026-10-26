# Kindle Hack

给已越狱的旧 Kindle 装上微信读书、DOOM 和 Neo Geo 游戏。保留原厂系统，用 KUAL 菜单打开和退出。

## 三步安装

1. Kindle 已越狱并装好 **KUAL**。还没准备好，先走 [KindleModding 官方向导](https://kindlemodding.org/jailbreak-wizard.html)。
2. 在[最新 Release](https://github.com/OceanJy333/kindle-hack/releases/latest) 下载 `kindle-hack-ai-kit.zip`，解压，把里面的 **`extensions` 文件夹复制到 Kindle USB 根目录**。已有同名文件夹时合并。
3. 安全弹出 Kindle、拔下 USB，打开 **KUAL → Ocean → 安装 / 检查**。检查后从同一菜单打开微信读书或游戏。

微信读书在设备上扫码登录自己的账号。DOOM 的 `doom1.wad`、Neo Geo 的 `mslug.zip` 和 `neogeo.zip` 由使用者自备，位置见[设置指南](docs/setup.md)。

## 交给 AI 安装

具备**访问本地文件、执行命令**能力的桌面 AI 可以按固定步骤安装。普通聊天窗口可以讲解步骤。把这段完整发给能操作电脑的 AI：

```text
请帮我安装 https://github.com/OceanJy333/kindle-hack 的最新懒人包。
先读取仓库 docs/给AI的安装任务.md 和 AGENTS.md，严格执行其中的安装合同。
下载最新 Release 的 kindle-hack-ai-kit.zip 与 SHA256SUMS，核对 SHA-256 后，
自动识别已通过 USB 连接的 Kindle，安装预编译文件。不编译，不配置 SSH，
不让我填写型号、固件或系统。需要我插拔 USB、点 KUAL 或扫码时再叫我。
安装后回读 ocean/state/status.json，分别报告阅读器和游戏的结果。
```

完整的固定步骤和错误处理见[给 AI 的安装任务](docs/给AI的安装任务.md)。

## 机型范围

| 设备 | 当前范围 |
| --- | --- |
| Paperwhite 1（PW1） | 单独游戏候选 profile，必须通过本机静态运行、显示和输入探针。其常见 Linux 2.6.31 内核不满足当前 Qt 运行时的 Linux 3.0 最低要求，原生阅读器返回 `reader_runtime_unavailable` |
| Paperwhite 2 / 3、Voyage | 已做显示、触摸与运行库预适配；本轮安装包待各型号实机验证 |
| 其他型号或未知固件组合 | 自动检查；不匹配的 profile 或探针失败会报告原因 |

历史 Voyage 实验有读书和游戏的实机记录。该记录与本次公共安装包的验证分开；不能据此认定所有机型、固件均已兼容。

菜单包含微信读书、DOOM、Neo Geo、返回原厂桌面、查看状态和卸载。应用运行时保留原厂电源服务；按电源键休眠会退出应用，唤醒后回原厂，需要时从 KUAL 重新打开。休眠行为待实机验证。

## 源码与开发

公共阅读器使用本仓库自主 QML 界面，运行包带 Qt、字体和 CA 证书，无需微信读书官方包的界面资源。账号和离线书库留在设备；卸载保留账号、书库和游戏存档。

- [设置指南](docs/setup.md)：文件位置、验证、卸载与错误处理。
- [Kindle Setup Skill](.agents/skills/kindle-setup/SKILL.md)：AI 执行入口。
- [BUILD.md](BUILD.md)：源码构建及 Release 打包。
- [本轮验证](docs/release-validation.md)：本地检查与待完成的设备实测。
- `weread/`、`doom/`、`arcade/`、`games/`、`native/`：阅读、游戏和墨水屏显示。
- `system/`：Voyage 独立 Linux 实验，开发者按[单独构建说明](system/BUILD.md)进入。

旧的官方资源提取、私有镜像和独立系统构建仍保留在开发者入口。普通 USB 安装走上述三步。

上游包括 [doomgeneric](https://github.com/ozkl/doomgeneric)、[FBA 2012 Neo Geo](https://github.com/libretro/fbalpha2012_neogeo)、[Qt](https://www.qt.io/)、[Kindle GPL 内核](https://github.com/katadelos/linux-3.0.35-lab126) 和 [barebox](https://www.barebox.org/)。各组件按其许可证使用。
