---
name: kindle-setup
description: 给已越狱 Kindle 安装本仓库的预编译 USB 懒人包；校验 Release、复制 KUAL 扩展、回读设备探针和应用结果。开发构建仅在用户明确要求时进入。
---

# Kindle Setup

仓库根目录是本 Skill 向上三级。先读 [README](../../../README.md) 和[给 AI 的安装任务](../../../docs/给AI的安装任务.md)，后者是普通安装的执行合同。操作前告诉用户将使用此 Skill。

## 普通安装

1. 从 GitHub 最新 Release 取得 `kindle-hack-ai-kit.zip` 和 `SHA256SUMS`，核对文件 SHA-256。没有匹配的预编译资产或校验失败时按合同停止该步，不尝试编译替代。
2. 自行识别已挂载 Kindle USB 根目录。按合同运行发行包的 `install.py`，或复制 `extensions/ocean`。保留其他扩展与用户数据。
3. 请用户安全弹出、拔 USB，点击 **KUAL → Ocean → 安装 / 检查**。尚未越狱或没有 KUAL 时，打开[当前官方向导](https://kindlemodding.org/jailbreak-wizard.html)，按对应的越狱后设置补齐。
4. 用户重新连 USB 后，读取 `ocean/state/status.json` 和合同指定的日志。按组件状态处理；需要用户实测时请其打开功能并反馈。扫码由用户完成。

不要先索要型号、固件、系统和 SSH 信息。设备探针决定 profile 与运行库可用性；未知 profile、`unsupported_kernel`、失败探针按报告处理。不要把文件复制成功或 `ready` 写成真机读书/游戏通过。

## 固定边界

- 使用预编译 ZIP；普通安装不走 `native/device-start.sh`、`native/physical-desktop.sh`、旧私有 ext2 镜像或独立系统入口，也不停止原厂 `powerd`。
- PW1 走独立游戏候选 profile，静态运行、显示与输入探针均需通过。Linux 2.6.31 不满足当前 Qt 的 Linux 3.0 最低要求；`reader_runtime_unavailable` 按明确不支持处理。
- PW2、PW3、Voyage 属于预适配、待本轮包的各型号实机验证。其他型号以现场 profile 和探针结果为准。
- 公共阅读器为自主 QML，无需提取官方包资源；用户扫码登录自己的账户。WAD、ROM 和 BIOS 使用者自备。
- 原厂电源服务保持运行；手动休眠会结束应用会话，唤醒回原厂，再从 KUAL 打开。实际睡眠、唤醒和退出结果另行验证。
- 没有本地操作能力的聊天 AI，只提供 README 的三步操作，不声称已经下载或安装。

## 开发任务

用户明确要求编译、移植或独立系统时，才读 [BUILD.md](../../../BUILD.md)、[weread/BUILD.md](../../../weread/BUILD.md) 或 [system/BUILD.md](../../../system/BUILD.md)。沿用该任务的真实设备信息和现有布局。普通安装失败不会自动扩大为开发或刷写任务。

## 交付

简短列出安装位置、KUAL 启动方法、设备报告、实际验证结果和未完成的实体操作。卸载保留的账号、离线书库、游戏数据见[设置指南](../../../docs/setup.md)。
