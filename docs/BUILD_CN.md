# 编译与测试

## 固定版本

- Python 3.10+；ESP-IDF 必须为 `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`，目标 esp32s31。
- utils `be151ae047014e79a9c7ae0092dd203844499003`，BSP `5f88b91e1be055d1d3674364123b2779a1758377`。Git 子模块已固定提交，不更新到 latest。
- GSP 1.5.1、GSPC 0.6.1；其他组件由应用 dependencies.lock 固定。

本仓库保持官方工作区路径：应用在 projects/learning_assistant；公共工具/板级代码在 submodule。初次克隆执行 `git submodule update --init submodule/esp-mosaico-utils submodule/esp-mosaico-bsp`。不要用 Recovery 工程作为应用模板。

## ESP-IDF

先按匹配版本工具文档安装 SDK 和 Python 依赖，核对 `git -C <IDF路径> rev-parse HEAD`。激活 SDK 环境后，在应用目录使用官方构建工具；Windows 已提供 `tools/local.ps1 build` 快捷入口，默认 SDK 在工作区 .codex-runs/esp-idf-mosaico，工具缓存 .codex-runs/idf-tools。其他平台可使用已激活、已核对提交的 IDF 环境构建，不使用任何裸 flash 命令。

构建之后还需要生成完整系统包，不能把旧 irisfw 当作本次新构建产物。准备安装时，官方 `python mosaico.py iris system-update --project projects/learning_assistant` 会准备锁定的 Iris 主机 Python 环境并构建完整包；该命令之后会实际连接和更新设备。

仅离线打包时，先准备 utils 的锁定 Iris 主机 Python 环境，通过 CMake 变量或环境变量 `ESP_IRIS_PYTHON` 指向该 Python，再重新配置应用。从应用目录执行 `cmake --build build --target system-update-bundle`，输出 `build/learning_assistant-system-update.irisfw`。该目标调用固定上游的 prepare_system_update.py 和 bundle builder，不连接设备。资源变化需完整 system-update，不单独覆盖 app。

应用保留 Vibe Mode 0.2 的不可变前缀：`vibe_mode` 为 test 分区，普通应用位于 `main_app`，OTA writer 仅在 Vibe Mode 中。调用 `iris_ota_support_start()`，使用 ABI 2、`esp-mosaico/v2` 和 `esp-iris-system-update/0.2`。不能修改旧包版本字符串代替重建。SDK、工具和字体生成环境影响二进制，不承诺与发布固件逐字节一致；发布固件 SHA-256 见 firmware/SHA256SUMS.txt。

## PC 模拟与单元测试

安装依赖后，使用 `python mosaico.py project sim --project projects/learning_assistant --interactive` 生成共享原生预览。该模拟器使用标记的演示数据，不访问硬件或真实模型。Windows 快捷入口为应用 tools/local.ps1 sim。

共享 C 测试目标：learning_model_test、provider_protocol_test、orientation_logic_test、viewport_test、speech_conversation_test、controls_test、questions_test。模拟工程首次配置完成后，`tools/local.ps1 test` 编译并运行这些测试；测试通过不替代设备实测。

ui/main.json、已生成图标和字体已包含，普通构建无须重新生成美术源图。tools/prepare_camera_voice_art.py 和 extract_design_icons.py 是开发期重建助手，引用未公开的设计原稿，不属于构建前置条件。字体许可证单独保留。

## 安装

按 [安装说明](../firmware/INSTALL_CN.md) 使用匹配的 0.2 工具和设备；不执行整片擦除，不覆盖身份/凭据，不抢占其他 Gateway 客户端。旧版 ABI 1 设备须先按官方迁移流程安装 0.2 基础固件，普通 System Update 不跨 ABI 升级。
