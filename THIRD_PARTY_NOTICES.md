# 第三方来源与许可

仓库提交源码与构建配置。依赖下载到被 Git 忽略的 `build/`。作者代码的 MIT 授权范围见 [LICENSE](LICENSE)。下列软件和资源各自保留原许可。

| 依赖 | 固定来源 | 许可与范围 |
| --- | --- | --- |
| doomgeneric | `ozkl/doomgeneric`，`e257a09527062bfb986b6e5e0780839b6cbd5b71` | GNU GPL v2，原文 [GPL-2.0.txt](LICENSES/GPL-2.0.txt)。链接生成的 DOOM 程序应按上游 GPL 要求提供对应源码。 |
| FB Alpha 2012 Neo Geo | `libretro/fbalpha2012_neogeo`，`e092097037d494c50d17c5144a2758d68538061b` | FB Alpha / Final Burn 与各组件许可；含非商业条款，原文 [FB-Alpha.txt](LICENSES/FB-Alpha.txt)。构建脚本公开静音与跳帧改动。 |
| Noto Sans CJK SC | `notofonts/noto-cjk`，Sans 2.004，`523d033d6cb47f4a80c58a35753646f5c3608a78` | SIL Open Font License 1.1。只下载 Regular、Medium、Bold。 |
| Zig | 0.15.2，ziglang.org 官方发布包 | Zig MIT；捆绑组件另见下载包内 LICENSE 与 lib/ 的许可。 |
| zlib | 1.3.1，zlib.net 官方源码包 | zlib License。 |
| npm 依赖 | `native/package-lock.json` 固定版本和完整性校验 | `@resvg/resvg-js`（MPL-2.0）；`fontkit`、`fast-xml-parser`（MIT）；间接依赖许可见各包。 |
| Linux / ARM kexec | `system/` 构建脚本固定的上游源码 | GNU GPL v2；`system/kexec/` 包含 Linux 派生实现。 |
| Qt 6 / glibc / OpenSSL / libqrencode | `weread/BUILD.md` 的 Debian snapshot 与源码构建 | Qt 开源模块按 LGPL v3 / GPL v3 等模块许可；glibc LGPL v2.1+；OpenSSL 3 Apache-2.0；libqrencode LGPL v2.1+。原文随源码/包提供。 |
| BusyBox / Dropbear / wpa_supplicant | `system/BUILD.md` 与获取脚本 | BusyBox GPL v2；Dropbear MIT 风格多条款；wpa_supplicant BSD。保留各源码包许可。 |

获取入口是 `python3 tools/fetch-deps.py`；依赖 URL 与 SHA-256 记录在 [dependencies.json](tools/dependencies.json)。系统与 Qt 依赖使用各自的构建入口。

Amazon 固件、官方微信读书安装包及提取资源由使用者自行提供。仓库只保留资源提取和补丁工具，以及作者自行编写的 Qt/QML 界面与适配。`patch_qml.py` 需要用户本地提取出的资源；提取产物留在 `build/`。

DOOM WAD、合金弹头 ROM、Neo Geo BIOS 不随仓库提供。构建游戏前端不需要这些游戏数据，运行时由使用者自行放入文档指定目录。游戏商标与内容归各自权利人所有。
