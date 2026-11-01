# Kindle Hack

Bring WeRead, DOOM and Neo Geo games to an old Kindle. The public USB installer keeps the stock Kindle system and launches apps through KUAL.

[中文说明](README.md) · [Latest release](https://github.com/OceanJy333/kindle-hack/releases/latest) · [Setup guide](docs/setup.md) · [Build from source](BUILD.md)

## Install in three steps

**Jailbreak your Kindle and install KUAL first.** Follow the [KindleModding jailbreak guide](https://kindlemodding.org/jailbreak-wizard.html) before using this installer.

1. Download `kindle-hack-ai-kit.zip` from the latest release and extract it.
2. Copy the `extensions` folder to the root of the Kindle's USB drive. Merge it with an existing folder if prompted.
3. Safely eject and unplug the Kindle. Open **KUAL → Ocean → 安装 / 检查** (“Install / Check”). Then launch WeRead or a game from the same menu.

Scan the login QR code with your own WeRead account. Supply your own `doom1.wad`, `mslug.zip` and `neogeo.zip`; file locations are listed in the [setup guide](docs/setup.md).

## Let your AI assistant help

Use an assistant that can access local files, operate your computer and run commands. Give it this task:

```text
Install the latest prebuilt USB kit from https://github.com/OceanJy333/kindle-hack.
First read AGENTS.md, docs/给AI的安装任务.md and
.agents/skills/kindle-setup/SKILL.md, and follow their installation contract.
Download kindle-hack-ai-kit.zip and SHA256SUMS from the latest release.
Verify SHA-256, detect my USB-connected Kindle, and install the prebuilt files.
Do not compile, configure SSH, or ask me to identify my model or firmware manually.
Ask me when I need to connect or eject USB, tap KUAL, or scan a QR code.
Read back ocean/state/status.json and report reader and game results separately.
After device testing, exit the app and return to the stock desktop before reconnecting USB.
```

The menu includes WeRead, DOOM, Neo Geo, return to stock desktop, status and uninstall. Uninstall preserves accounts, downloaded books and game saves. The apps retain the stock power service; the intended sleep behavior is to exit the app and wake into the stock desktop, with physical-device sleep validation still pending.

## Device support and validation

| Device | Current scope |
| --- | --- |
| Paperwhite 1 | A separate games-only candidate profile must pass device probes. Its common Linux 2.6.31 kernel does not meet the current Qt runtime's Linux 3.0 minimum; the native reader reports `reader_runtime_unavailable`. |
| Paperwhite 2 / 3 and Voyage | Display, touch and runtime profiles are prepared. The current public installer still needs physical-device validation on each model. |
| Other models or unknown firmware combinations | The installer checks device capabilities and reports mismatches or failed probes. |

Earlier Voyage experiments have real-device reading and gameplay evidence. Those experiments and validation of the current public installer are tracked separately.

## What is in the repository?

- `weread/`: the public reader uses this project's own QML interface. Its runtime includes Qt, fonts and CA certificates; it does not need interface assets from the official WeRead app.
- `doom/`, `arcade/`, `games/`, `native/`: games, input and e-ink display support.
- `system/`: a separate experimental Linux system for Voyage. See [its build instructions](system/BUILD.md).
- [Release validation](docs/release-validation.md): checks completed and device tests still pending.

The video shows the earlier development experiments, including reverse engineering and a custom system. The ordinary USB installation above keeps the stock system.

Upstream projects include [doomgeneric](https://github.com/ozkl/doomgeneric), [FBA 2012 Neo Geo](https://github.com/libretro/fbalpha2012_neogeo), [Qt](https://www.qt.io/), the [Kindle GPL kernel](https://github.com/katadelos/linux-3.0.35-lab126) and [barebox](https://www.barebox.org/). Each component follows its own license.
