# 编译与测试

## 固定版本

- Python 3.10+；ESP-IDF 必须为 `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`，目标 esp32s31。
- utils `abd926592bd9a7119e245faf5a56e6ebc78a8369`，BSP `9d12a91046a1c88bbcb2e3876a0289502eb4bd3b`。Git 子模块已固定提交，不更新到 latest。
- GSP 1.4.0、GSPC 0.5.0；其他组件由应用 dependencies.lock 固定。

本仓库保持官方工作区路径：应用在 projects/learning_assistant；公共工具/板级代码在 submodule。初次克隆执行 `git submodule update --init submodule/esp-mosaico-utils submodule/esp-mosaico-bsp`。不要用 Recovery 工程作为应用模板。

## ESP-IDF

先按匹配版本工具文档安装 SDK 和 Python 依赖，核对 `git -C <IDF路径> rev-parse HEAD`。激活 SDK 环境后，在应用目录使用官方构建工具；Windows 已提供 `tools/local.ps1 build` 快捷入口，默认 SDK 在工作区 .codex-runs/esp-idf-mosaico，工具缓存 .codex-runs/idf-tools。其他平台可使用已激活、已核对提交的 IDF 环境构建，不使用任何裸 flash 命令。

构建之后还需要生成完整系统包，不能把旧 irisfw 当作本次新构建产物：在应用构建目录运行 `cmake --build build --target system-update-bundle`（从应用目录执行；相应 build 目录需已存在）。资源变化需完整 system-update，不单独覆盖 app。

应用持续保留官方不可变 Recovery 前缀与 OTA writer 分工。SDK、工具和字体生成环境影响二进制，不承诺与已发布固件逐字节一致；发布固件 SHA-256 见 firmware/SHA256SUMS.txt。

## PC 模拟与单元测试

安装依赖后，使用 `python mosaico.py project sim --project projects/learning_assistant --interactive` 生成共享原生预览。该模拟器使用标记的演示数据，不访问硬件或真实模型。Windows 快捷入口为应用 tools/local.ps1 sim。

共享 C 测试目标：learning_model_test、provider_protocol_test、orientation_logic_test、viewport_test、speech_conversation_test、controls_test、questions_test。模拟工程首次配置完成后，`tools/local.ps1 test` 编译并运行这些测试；测试通过不替代设备实测。

ui/main.json、已生成图标和字体已包含，普通构建无须重新生成美术源图。tools/prepare_camera_voice_art.py 和 extract_design_icons.py 是开发期重建助手，引用未公开的设计原稿，不属于构建前置条件。字体许可证单独保留。

## 安装

严格按 firmware/INSTALL_CN.md 使用匹配旧版工具和设备；不执行整片擦除，不覆盖身份/凭据，不抢占其他 Gateway 客户端。当前包不面向 Vibe Mode 0.2 设备。
