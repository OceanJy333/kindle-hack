# v0.2.0-ai-kit 验证记录

2026-10-27，本轮重新编译了 USB 安装包，没有连接真实 Kindle。

| 检查 | 结果 |
| --- | --- |
| 原有主机测试 | 22 项通过 |
| USB 安装器 | 19 项通过：复制、重装、空间不足、卸载保留数据、机型与内核选择、缺资源拒绝交接、退出与异常恢复 |
| 显示与输入 | Linux 实际运行几何 6 组合、EPDC 模拟 7 组合、单点 / Type A / Type B 输入 9 组合；另有 ASan / UBSan 检查 |
| 自主阅读 UI | 真实 Qt 6 测试 20 项通过，无 QML 警告；审看 8 个界面。控制器使用测试数据 |
| ARM 程序 | DOOM、Neo Geo、设备探针和 Qt 客户端交叉编译成功；游戏为 Cortex-A8 / Linux 2.6.31 静态目标 |
| 游戏运行探针 | Cortex-A8 QEMU 下 DOOM、Neo Geo 探针成功，不刷新屏幕 |
| Qt 运行包 | 自带 glibc 的 ABI note 为 Linux 3.0.0；Cortex-A8 QEMU 下文件、随机数、线程、计时器和 TLS 全部成功 |
| 网络 | 同一 ARM 运行包加载随包 CA，访问 `https://ink.qq.com/` 返回 HTTP 200 |
| 第三方来源 | 运行库按准确 Debian 包、字节哈希与对应源码核对，源码和许可随附件提供 |

显示测试模拟 ioctl 和输入事件，不会操作墨水屏。QEMU 使用电脑上的 Linux 内核，不能代替 Kindle 旧内核、触屏、休眠和账号实测。

| 机型 | 本轮硬件结果 |
| --- | --- |
| PW1 | 游戏 profile 待设备探针与实测；当前原生阅读器不支持 Linux 2.6.31 |
| PW2 / PW3 / Voyage | 预适配完成，实际安装、扫码、翻页、游戏操作、退出、睡眠与重启后启动均待各型号验证 |

发行包的 `manifest.json` 保持 `hardwareValidation: pending`。最终 ZIP 及源码附件的 SHA-256 见同一 Release 的 `SHA256SUMS`。
