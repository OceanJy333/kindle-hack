# Kindle Hack：给 AI 的入口

用户说“帮我安装、设置 Kindle”时，执行 [docs/给AI的安装任务.md](docs/给AI的安装任务.md)，并读 [.agents/skills/kindle-setup/SKILL.md](.agents/skills/kindle-setup/SKILL.md)。默认安装最新 Release 的预编译 USB 包：校验 → 复制 `extensions/ocean` → 用户点 KUAL → 回读状态。

自动识别 USB 路径；设备侧探针识别型号、固件和能力。不要先让用户填写型号、固件、电脑系统。只向用户请求插拔 USB、KUAL 点击和扫码等实体操作。没有本地文件和命令能力时，明确交给用户三步安装说明。

普通安装无需 SSH、编译、官方客户端资源或换系统。包缺失、校验失败或探针失败，按安装任务中的固定分支报告；不要自行改用源码构建、旧 `native/device-start.sh`、停原厂 `powerd` 或刷写固件。缺少越狱/KUAL 时，回到当前 KindleModding 官方向导。

只有用户明确要求开发、源码构建、旧私有阅读器或独立系统时，才进入 [BUILD.md](BUILD.md) 及对应模块文档。引导器、分区和内核刷写是单独任务。

保护无关修改。构建产物和用户资源放 `build/`；不提交设备凭据、账号、离线书籍、官方安装包、固件、WAD、ROM 或 BIOS。分别陈述文件复制、设备检查、实机功能和重启后启动的结果；未知结果直接写待验证。
