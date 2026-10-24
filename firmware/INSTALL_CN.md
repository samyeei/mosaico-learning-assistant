# 安装学习助手 v1.1.0（Vibe Mode 0.2）

硬件：ESP-Mosaico / ESP32-S31，16 MiB Flash。拍题需要左侧摄像头插件。完整包包含应用、分区表和 GSP 界面资源，不包含 API Key、Wi-Fi 密码或私人设备数据。

## 通过 mosaico.ai 安装

1. 确认设备已安装 Vibe Mode 0.2 或更新且兼容 ABI 2 的基础固件。关机后按住顶部 AI 键开机进入 Vibe Mode；这里不是按住 Boot 键的黑屏 ROM 模式。
2. 按 Vibe Mode 的界面提示完成联网。在 mosaico.ai 找到“Mosaico 学习助手”，确认附件版本为 v1.1.0 的 `.irisfw`。
3. 项目审核通过后，平台会生成六位安装码。使用项目页面实际公布的码，在设备 Vibe Mode 安装入口中输入并安装。尚待审核或未显示安装码时，不代表在线安装已经开放；普通 ZIP 附件不会生成安装码。
4. 安装及重启期间保持供电和网络。打开首页后移动设备检查实时预览，再试触摸、拍照选题、语音识别和朗读。
5. 首次使用进入连接设置，手机连接设备的免密 Learn-Setup 热点，在 http://192.168.4.1 设置家庭 Wi-Fi 和自己的百炼 API Key。完整步骤见 [使用教程](../docs/USAGE_CN.md)。

若网页提供其他直接安装入口，按该页面和当前官方说明操作；不要将 `.bin` 或旧 ZIP 改扩展名冒充 `.irisfw`。

## 用官方 0.2 工具安装完整包

本仓库固定 utils `be151ae047014e79a9c7ae0092dd203844499003`、BSP `5f88b91e1be055d1d3674364123b2779a1758377`。初始化这两个子模块后，在仓库根目录执行官方 `python mosaico.py`；不要使用旧版工具或自行更新到不匹配的 latest。

先检查所有权与设备，再根据当前目录和设备数量选择工程及同一设备：

```powershell
python mosaico.py iris status --all --json
python mosaico.py iris list --project projects/learning_assistant --details --json
python mosaico.py iris system-update --bundle "完整绝对路径/mosaico-learning-assistant-v1.1.0.irisfw"
```

通过高速 USB 或官方支持的独立网络数据链路更新；仅有 UART / USB Serial-JTAG 日志链路时不能执行 Iris 固件更新。不要抢占其他客户端，不要在未完成的写入期间启动第二次写入。目标应报告 `esp-mosaico/v2`、ABI 2 和 `mosaico-retained-test-2m-v2`。

旧 ABI 1、空白或基础固件不明的设备，先读 [迁移说明](../docs/MIGRATION_02_CN.md)，通过官方 recover 流程处理基础固件。不要整片擦除、覆盖身份或凭据、直接调用底层刷写工具，也不要降级基础固件来装旧版应用。

## 文件与校验

- 新版：`mosaico-learning-assistant-v1.1.0.irisfw`，格式 `esp-iris-system-update/0.2`。
- SHA-256：见同目录 `SHA256SUMS.txt`；下载后核对文件名与校验值。
- v1.0.0 `.irisfw` 与 manual ZIP 是 ABI 1 旧版存档，只用于已验证的旧环境。其说明保存在 [旧版安装文档](LEGACY_V1_INSTALL_CN.md)，不用于本次在线安装。

安装完成后核对设备身份、更新记录和新 Boot ID；能够重连不能替代相机、麦克风、扬声器、联网与操作验收。
