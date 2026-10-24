# 从旧版迁移到 Vibe Mode 0.2

学习助手 v1.1.0 使用 Vibe Mode 0.2 / Recovery ABI 2；v1.0.0 的 ABI 1 完整包只作为旧版存档保留。两个版本不能混用安装工具或基础固件。

## 已经是 0.2 的设备

使用匹配的 0.2 官方工具安装 v1.1.0 完整 `.irisfw`。该包包含普通应用、完整分区表和界面资源；只更新应用 bin 会丢失相应资源。网站安装方式见 [安装说明](../firmware/INSTALL_CN.md)。

## 仍为 ABI 1 的设备

先按[官方迁移指南](https://github.com/esp-mosaico/esp-mosaico-utils/blob/be151ae047014e79a9c7ae0092dd203844499003/mosaico-tools/docs/migration-0.2.md)升级基础固件，再安装应用。此处不提供把旧包强制改成新包的方法。

1. 在旧工具环境保存设备身份、日志和有效的崩溃记录，保留已验证的旧版完整安装包；退出本次持有设备的调试客户端，确认没有其他客户端或未完成的写入。
2. 使用本仓库固定的 0.2 工具，先检查 `python mosaico.py iris status --all --json` 和 `python mosaico.py iris list --project projects/learning_assistant --details --json`。0.2 不会接管仍运行的旧 Gateway，也不会使用旧版本的验证缓存。
3. 旧 ABI 不能通过普通 System Update 跨代迁移。运行 `python mosaico.py recover --help`，按该固定版本官方恢复流程使用已评审基础包。仅在工具要求时，按住 USB-C 左侧 Boot 键开机进入 ROM 下载模式；保持连接同一台设备。
4. 恢复流程应只写入新基础固件所需范围，保留 sysmeta/NVS、设备身份及用户配置；禁止整片擦除。若工具提出擦除或无法确认目标身份，先停止并诊断。不要自行调用底层串口写入工具。
5. 核对实时 Device ID、Vibe Mode 版本、ABI 2、`esp-mosaico/v2`、`mosaico-retained-test-2m-v2` 与可用更新能力。当前固定上游的评审基础包为 0.2.1。
6. 通过 `python mosaico.py iris system-update --project projects/learning_assistant` 安装新构建，或用 `--bundle` 安装发布包。确认新 Boot ID 和正常应用模式，再检查首页、相机、语音、朗读、触摸、旋转和保存配置；重连成功不能替代功能检查。

## 开发者集成变化

- ESP-IDF 提交仍固定为 `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`；GSP 升级至 1.5.1，GSPC 至 0.6.1。
- 应用集成入口改为 `esp-mosaico-vibe-mode/cmake/mosaico_idf_project.cmake`，保留 `esp_mosaico_app_recovery` 与 `iris_ota_support_start()`。
- 保留分区改为 `vibe_mode` / `app,test`，普通应用为 `main_app` / `app,ota_0`；固定前缀位置不变。
- 应用资源区仍为 0xd00000 起的 3 MiB，避免覆盖现有数据；使用完整系统更新安装资源与布局。
- 启用 QIO 和两个 TinyUSB CDC 接口，遵循官方生成的写入参数。应用中没有 OTA writer。

迁移基础固件后不应直接刷回旧版应用；需要回退时也要使用隔离的旧工具和匹配基础布局，重新核对写入范围与配置保留条件。
