# Kindle Hack

把旧 Kindle 变成能读微信读书、玩 DOOM 和合金弹头的小设备。

这里整理了 Ocean 在 Kindle Voyage 上做的原生 Qt 阅读器、DOOM / 合金弹头适配、墨水屏刷新和独立 Linux 系统源码。想照着做，可以把这个仓库交给 AI，让它按你的设备一步步设置。

## 最简单的用法

把下面这段发给能读 GitHub、运行本地命令的 AI 编程助手：

```text
请读取 https://github.com/OceanJy333/kindle-hack，先读 AGENTS.md 和
.agents/skills/kindle-setup/SKILL.md，帮我设置 Kindle。

我的型号是：____
固件版本是：____
是否已越狱：____
电脑系统是：____
我想用的功能是：微信读书 / DOOM / 合金弹头（选需要的）。

先确认机型、系统和连接方式，再执行适合这台设备的构建与安装。
```

如果 AI 在本机工作，先克隆仓库：

```sh
git clone https://github.com/OceanJy333/kindle-hack.git
cd kindle-hack
```

Skill 随仓库提供。能读取文件的 AI 可直接使用，无需先安装进全局技能目录。

## 先越狱，再设置

运行这些程序需要已越狱的 Kindle，以及可以执行 root 命令的入口。安装和调试使用 SSH（远程终端连接）。

还没越狱时，先查看 [KindleModding 入门](https://kindlemodding.org/jailbreaking/) 和[越狱向导](https://kindlemodding.org/jailbreak-wizard.html)。按自己的机型、固件和注册状态选择方法，再完成对应的越狱后设置。越狱和换系统是两件事；保留原厂系统也可以安装适配程序。

这个项目的硬件适配以 **Kindle Voyage、ARMv7、原厂固件 5.13.6** 为起点。Ocean 的设备后来运行自编译的 `3.0.35-ocean-voyage` 内核。其他型号要核对显示、输入、处理器和系统库后再移植。

## 有什么

| 功能 | 内容 |
| --- | --- |
| 微信读书 | 原生 Qt 客户端适配，扫码登录、书架、阅读、双向进度同步、划线段落评论和离线缓存 |
| DOOM | doomgeneric 的 Voyage framebuffer、触摸和 PagePress 适配 |
| 合金弹头 | FBA 2012 Neo Geo 核心的 ARMv7 适配，触屏按键与 PagePress |
| 墨水屏刷新 | 游戏共用独立显示进程、A2 快速黑白刷新与 GC16 清残影 |
| 独立系统 | Voyage 的内核、rootfs、引导、无线、SSH 和电源管理构建工具 |

DOOM、合金弹头、阅读刷新、手机与 Kindle 双向阅读进度和段落评论，已有 Ocean 的 Voyage 实机使用记录。独立系统也曾进入自编译内核。新设备仍需逐项验证安装、显示、输入和重启后的行为。

仓库还保留了本机首页、日报和电脑任务状态接入代码。

## 从哪里开始

- **让 AI 帮你设置**：[Kindle Setup Skill](.agents/skills/kindle-setup/SKILL.md)
- **安装顺序与设备检查**：[设置指南](docs/setup.md)
- **源码、外部输入与构建命令**：[BUILD.md](BUILD.md)
- **各模块**：`weread/`、`doom/`、`arcade/`、`games/`、`native/`、`system/`

仓库提供自写代码、适配补丁和提取工具。微信读书官方安装包、原包 QML / 字体、Kindle 固件、设备面板数据、游戏 ROM / BIOS 由使用者自行准备，来源和用途见 [BUILD.md](BUILD.md)。账号在自己的设备扫码登录，SSH 密钥在自己的电脑生成。

上游来源包括 [doomgeneric](https://github.com/ozkl/doomgeneric)、[FBA 2012 Neo Geo](https://github.com/libretro/fbalpha2012_neogeo)、[微信读书墨水屏入口](https://ink.qq.com/)、[Kindle GPL 内核](https://github.com/katadelos/linux-3.0.35-lab126) 和 [barebox](https://www.barebox.org/)。各组件按其许可证使用。
