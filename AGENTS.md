# Kindle Hack：给 AI 的入口

用户要求配置 Kindle、安装阅读器或游戏时，先读 [.agents/skills/kindle-setup/SKILL.md](.agents/skills/kindle-setup/SKILL.md)，再按目标读 [docs/setup.md](docs/setup.md) 和 [BUILD.md](BUILD.md)。

先取得用户的机型、固件、是否越狱和电脑系统。原厂系统与 Voyage 自编系统走各自入口。普通功能安装以用户选择为范围；引导器、分区和内核刷写另作明确任务。

构建产物和用户资源放 `build/`，不提交设备凭据、账号、书籍缓存、官方安装包、固件镜像或 ROM / BIOS。保护用户已有配置与工作区修改。设备验证完成后，说明实际通过的项目和仍未验证的项目。
