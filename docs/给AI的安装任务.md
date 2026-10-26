# 给 AI 的安装任务

把下面整段交给能访问本地文件、执行命令的桌面 AI。用户不用先填型号、固件、电脑系统。

```text
请在我的已越狱 Kindle 上安装 Ocean Kindle Hack 懒人包。
仓库：https://github.com/OceanJy333/kindle-hack

你负责下载、SHA-256 校验、识别 USB 路径、复制和读取结果。
先读仓库 AGENTS.md、.agents/skills/kindle-setup/SKILL.md 和
 docs/给AI的安装任务.md，然后严格执行本文的“执行合同”。
默认使用 GitHub 最新 Release 的 kindle-hack-ai-kit.zip 和 SHA256SUMS。
不用 SSH，不编译，不提取官方客户端，不换系统，不停原厂 powerd。
需要我插拔 USB、点 KUAL 或扫码时，用一句话告诉我具体操作。
完成后分别报告文件复制、设备检查和功能实测结果。
```

## 执行合同

按顺序做，不自行发明命令或改用旧安装入口。

1. **确认本地操作能力。** 你需要下载文件、读写 USB 盘、执行本机命令。只有聊天能力时，给用户 [README 的三步安装](../README.md#三步安装)，明确尚未操作电脑。
2. **取得发行文件。** 打开 `https://api.github.com/repos/OceanJy333/kindle-hack/releases/latest`，从响应的 `assets` 中下载名称完全匹配的 `kindle-hack-ai-kit.zip` 和 `SHA256SUMS`，保存到同一工作目录。缺少任一资产就报告缺少预编译发行包，结束该步，不转为源码编译。
3. **核对 SHA-256。** 从 `SHA256SUMS` 读取 `kindle-hack-ai-kit.zip` 对应的 64 位哈希，与实际 ZIP 哈希逐字比较。任选当前系统已有的一条固定命令：macOS `shasum -a 256 kindle-hack-ai-kit.zip`；Linux `sha256sum kindle-hack-ai-kit.zip`；Windows PowerShell `Get-FileHash .\kindle-hack-ai-kit.zip -Algorithm SHA256`。不一致时删除此次下载的 ZIP，重新下载一次；仍不一致就报告两者哈希并停止。
4. **解压并识别 USB。** 解压到独立工作目录，确认根目录有 `install.py` 和 `extensions/ocean/menu.json`。自行查看已挂载磁盘，找 Kindle USB 根目录；macOS 常见 `/Volumes/Kindle`，Linux 常见 `/media/<用户名>/Kindle`，Windows 是实际盘符。没有 Kindle 时，只请用户“用 USB 连接 Kindle，并进入文件传输模式”。多个 Kindle 时再请用户指出目标盘。
5. **复制。** 在发行包根目录执行下方 `install` 命令，再执行 `verify`。将命令中的 `KINDLE_USB_ROOT` 替换为你查到的实际绝对路径，不让用户填。没有 Python 3 时，用文件管理能力把发行包 `extensions/ocean` 完整复制到目标盘 `extensions/ocean`，合并 `extensions`，保留其他扩展。不要删除整个 `extensions` 或用户数据。
6. **设备检查。** 请用户“安全弹出 Kindle、拔掉 USB，打开 KUAL → Ocean → 安装 / 检查；完成后重新连 USB”。KUAL 不存在或尚未越狱时，打开当前 `https://kindlemodding.org/jailbreak-wizard.html` 和其越狱后设置，协助完成对应步骤后再继续。
7. **回读。** 执行 `status`，读取目标盘 `ocean/state/status.json`，按下表处理。错误时读取相应日志，报告实际值；不修改 profile、不跳过探针、不强行启动失败组件。
8. **功能实测。** 只有对应组件 `ready` 时，请用户安全弹出、拔 USB，再点 KUAL 的目标功能。阅读器扫码由用户完成。用户体验后先退出到原厂桌面，再连接 USB，读取 `session.json`、`app.log`；同时记录用户实际看到的启动、触摸和退出结果。文件复制或探针通过不能代替这一步。

## 固定命令

在**解压后的发行包根目录**执行，`KINDLE_USB_ROOT` 为 AI 已识别的盘根目录。Windows 有 Python Launcher 时用 `py -3` 代替 `python3`。

```sh
python3 install.py install --kindle "KINDLE_USB_ROOT" --json
python3 install.py verify --kindle "KINDLE_USB_ROOT" --json
python3 install.py status --kindle "KINDLE_USB_ROOT" --json
```

用户要求卸载时执行：

```sh
python3 install.py uninstall --kindle "KINDLE_USB_ROOT" --json
```

源码工作区对应入口为 `python3 installer/install.py`，普通安装优先使用发行包根目录的 `install.py`。

| 电脑命令退出码 | 含义与下一步 |
| --- | --- |
| `0` | 当前电脑操作成功。`install` 后继续 `verify` 和 KUAL 设备检查 |
| `1` | 查看 stderr JSON 的 `error`，或 `verify` 的 `missing`。未找到 USB 就请用户连接；缺文件就重新取完整发行包；`verify` 失败就重复制并再验证一次 |
| `2` | 命令参数有误。改为上面的固定格式 |

`verify.ok=true` 只表示程序文件齐全。`status.device.phase=awaiting_device_check` 表示尚未取得 KUAL 检查结果。

## 状态与下一步

结果文件相对 Kindle USB 根目录：`ocean/state/status.json`。

| 字段 / 状态 | 下一步 |
| --- | --- |
| `hardware=ready` | 显示、输入和静态运行探针通过；继续看各组件 |
| `hardware=unsupported_device_profile` | 报告未知设备配置，收集 `device.json`、`probe.json`、`probe.log`；结束安装验证 |
| `hardware=unsupported_kernel` | 报告内核不在当前 profile 范围，收集 `device.json`；PW1 需 `2.6.31`，PW2 / PW3 / Voyage 需 `3.0.35`，均接受版本后缀 |
| `hardware=hardware_probe_failed` | 收集 `probe.json`、`probe.log`，报告硬件检查失败 |
| `reader=ready` | 可进行扫码、开书、翻页和退出实测 |
| `reader=reader_runtime_unavailable` | 当前设备不运行此 Qt 阅读器。PW1 的 Linux 2.6.31 属于此状态；继续看游戏状态 |
| `reader=reader_payload_missing` | 重新下载完整发行包并复制 |
| `reader=reader_runtime_failed` | 读取 `reader-probe.json`、`reader-probe.log`，报告本地运行库检查失败 |
| `doom=ready` / `neogeo=ready` | 对应游戏可进行实测 |
| `doom=missing_doom1_wad` | 请用户提供其自备的 `doom1.wad`，复制到 USB 根目录 `games/doom1.wad`，再做 KUAL 检查 |
| `neogeo=missing_mslug_or_neogeo_zip` | 请用户提供匹配 FBA v0.2.97.29 的 `mslug.zip` 和 `neogeo.zip`，复制到 `games/roms/`，再检查 |
| `game_payload_missing` | 重新下载完整发行包并复制 |
| `game_runtime_failed` | 读取 `doom-probe.log` 或 `neogeo-probe.log`，报告该游戏运行检查失败 |
| 任一组件 `hardware_probe_failed` | 先处理 `hardware` 字段及硬件日志 |
| `sleepResume=unverified` | 尚无实机睡眠/唤醒验证，不改为通过 |

`status.json` 的 `next` 是固定操作码：

| `next` | 操作 |
| --- | --- |
| `open_kual_app` | 进行 KUAL 功能实测 |
| `copy_game_data_then_check` | 放入用户自备游戏数据，再点安装 / 检查 |
| `reinstall_release` | 重复制完整发行包，再检查 |
| `report_device_json` | 报告 `device.json` 的设备和内核结果 |
| `read_probe_logs` | 读取上表对应的探针日志 |

上表日志均在 `ocean/state/`。应用记录为 `session.json`、`app.log`、`sleep.log`。出现表外状态时，保留原值和日志，报告未完成的具体步骤。

## 交付格式

用短清单回答：安装盘和 Release 版本；复制与文件校验结果；profile 和各组件状态；用户实际完成的功能检查；下一项实体操作或失败原因。不要把“已复制”“已检查”“实机可用”合成一个成功结论。

卸载只移除 `extensions/ocean`。账号在 `ocean/state/weread/`，离线书库在 `weread-offline/`，游戏数据和存档在 `games/`，均保留。
