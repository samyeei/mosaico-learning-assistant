# Mosaico 学习助手

![Mosaico 学习助手封面：AI 视觉示意](docs/assets/cover.png)

把 ESP-Mosaico 变成可拍题、语音提问、分步讲解的随身学习助手。

**源码公开 · 禁止商业使用。** 原创代码采用 [PolyForm Noncommercial 1.0.0](LICENSE.md)。允许许可范围内的非商业学习、研究及修改；本项目未授予商业使用权。第三方字体、上游板级适配和依赖保留原许可，见 [第三方声明](THIRD_PARTY_NOTICES.md)。本项目属于 source-available，不以 OSI 开源许可证发布。

## 第一次使用

请从[安装说明](firmware/INSTALL_CN.md)和[完整使用教程](docs/USAGE_CN.md)开始：安装新版固件 → 获取百炼 API Key → 手机连接 Learn-Setup → 配置家庭 Wi-Fi / Token → 测试并保存 → 拍题选题与语音提问。

## 功能

- 拍题先识别题号；每页三题，点选后才讲解；同图换题与当前题追问。
- 横向实时取景、插件 LED 补光、180° 镜头朝向补偿；预览与拍照一致。
- 点击麦克风/声波，或按住顶部 AI 键录音；wave 动画；识别成文字后确认发送。
- 分页回答、手动朗读/停止；关闭自动朗读仍可手动播放。
- 顶部下拉调整亮度、音量、自动朗读和方向锁；向上滑收起。
- 一年级到高三；年级影响讲解提示，不绑定教材版本或学期。
- 手机连接设备热点，网页配置家庭 Wi-Fi 与自己的阿里云百炼 API Key。

## 下载与兼容性

**v1.1.0 面向 Vibe Mode 0.2 / ABI 2，使用真实重建的 `esp-iris-system-update/0.2` 完整包。** 平台审核通过并生成安装码后，用户可在 Vibe Mode 中安装；审核状态及六位码以 mosaico.ai 项目页为准。

- 新版完整包：`firmware/mosaico-learning-assistant-v1.1.0.irisfw`，校验值见 `firmware/SHA256SUMS.txt`。
- [安装前必读](firmware/INSTALL_CN.md)；旧版 ABI 1 设备先读[基础固件迁移说明](docs/MIGRATION_02_CN.md)，不能直接跨 ABI 更新。
- [旧版 v1.0.0 ZIP 存档](firmware/mosaico-learning-assistant-v1.0.0-manual.zip)只适用于匹配的旧环境，不用于新版在线安装；[旧版说明](firmware/LEGACY_V1_INSTALL_CN.md)。
- [编译与测试](docs/BUILD_CN.md)；[使用说明](docs/USAGE_CN.md)。
- 设备型号 ESP32-S31 / ESP-Mosaico；摄像头扩展板使用左侧插槽，麦克风/扬声器使用板载硬件。

## 界面

下图为原生选题页示意，题目为演示数据，非真实试卷识别结果。

![选题示意](docs/assets/questions-demo.png)

## 模型与隐私

设备使用百炼 Qwen 视觉、语音识别和语音合成接口。需要联网与自己的 API Key，费用由所配置账户承担；识别语音本身也会调用云服务。照片、录音及问题会发送给所配置的云模型。

仓库不包含 API Key、家庭 Wi-Fi 配置、设备 NVS、原始私人影片、环境日志或开发缓存。封面为 ChatGPT 生成的 AI 视觉示意，真实操作说明影片使用实机素材并配 MiniMax 中文旁白；影片配音不改变设备模型。

## 验证范围

新版 GSP 1.5.1 下七组 C 单元测试和 42 个共享原生界面场景通过。硬件验证、固件校验与平台提交状态分别记录在 [发布检查](docs/RELEASE_CHECKS.md)，不把构建或网页上传称为其他设备的实机验收。
