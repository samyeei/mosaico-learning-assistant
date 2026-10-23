# 安装前必读：Mosaico 学习助手 v1.0.0 旧版下载包

日期：2026-10-23。ZIP 是平台支持的普通下载附件，不会生成六位安装码。

## 兼容范围

- ESP32-S31 / ESP-Mosaico，16 MiB Flash。
- 已保留的 Recovery ABI 1、布局版本 4。本次实测 Recovery 0.1.2。
- 原始更新包 schema：esp-iris-system-update/v1。
- 不兼容 Vibe Mode 0.2 / Recovery ABI 2。不要降级基础固件、改包的 schema、修改分区表或整片擦除来尝试安装。
- 没有实测其他设备版本；设备身份、Recovery 状态或布局不明确时先诊断，不执行写入。

## 匹配工具

本次实测仓库：https://github.com/esp-mosaico/esp-mosaico-vibe
工作区基准 commit：df3faf0f99bfe92f942f5078b9818131dd38aba6
utils 实测 commit：abd926592bd9a7119e245faf5a56e6ebc78a8369
官方 utils：https://github.com/esp-mosaico/esp-mosaico-utils

使用已有匹配旧版的工作区/工具环境。新的 0.2 工具不能解析本包，不要使用 latest 替代。具体安装与设备维护遵循匹配版本仓库的 docs/mosaico-cli_CN.md。

## 安装流程

1. 解压到本机，校验下面的 SHA-256；本包已经包含应用、完整分区表和界面资源。
2. 使用旧版官方工具检查 `python mosaico.py iris status --all --json` 和 `python mosaico.py iris list --details --json`，确认唯一目标设备、设备身份、在线状态和匹配的基础布局；不要抢占其他客户端。
3. 确认上述兼容条件后，在该旧版工作区使用：

   python mosaico.py iris system-update --bundle "更新包的绝对路径/mosaico-learning-assistant-v1.0.0.irisfw"

   若存在多设备，按该版本官方文档选中已确认的同一设备。新设备/无法确认基础固件的设备不适用本下载包。不要只刷应用 bin，不使用原始串口工具绕过 Recovery。
4. 核对更新记录、重启后的设备身份与新的 Boot ID；回到学习助手后实测拍照、语音、触摸和联网。上传完成或重连成功本身不等于功能验收。
5. 手机连接设备显示的 Learn-Setup 热点，打开 192.168.4.1，填写家庭 Wi-Fi 与自己的百炼 API Key，测试通过后使用。勿公开自己的密钥。

## 校验

原始 .irisfw SHA-256：c7fe1a64582830280ff960194fbc235b34f742d1135e3a1316804fc410a3d7f8

本包未包含账号、API Key、Wi-Fi 配置、原始私人视频、开发缓存或环境凭据。
