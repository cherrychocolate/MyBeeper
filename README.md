# MyBeeper

AI personal-agent peripheral firmware snapshot. 这是经项目所有者确认可公开的代码与技术接手说明；完整会话、云端表格、内部产品资料和原始附件不在此公开仓库。

## 先读

1. `AGENTS.md`：接手规则。
2. `HANDOFF.md`：当前技术基线、验证边界与下一项工作。
3. `VERIFICATION.json`：原始快照的真实构建记录摘要。

## 当前代码结构

|目录|用途|已执行的检查|
|---|---|---|
|`firmware/esp-idf-e6-v01`＋兄弟`esp-epaper-v01`|E6 SPI/GPIO/FreeRTOS显示工程与便携协议核心|两配置S31编译；协议主机检查|
|`firmware/esp-hfp-build-v01/project`|HFP HF HCI示例与Classic/BLE启动分支|Classic/双模S31编译，已修应用分区|
|`firmware/esp-task-journal-v01`|任务规范记录、提交门控、NVS/RTOS互斥|S31编译、故障注入、进程/文件恢复检查|
|`firmware/esp-audio-pcm-v01`|ES8388控制与I2S0双工PCM、格式助手|两配置S31编译；仅格式助手主机检查|
|`firmware/esp-power-v01`|USB功率分配纯策略|边界与组合主机检查；不是CC硬件驱动|
|`firmware/esp-baseline-v01/firmware/task_core`|便携C任务状态核心|严格C11与主机行为检查|
|`legacy-reference/`|历史Python状态机|历史参考，非当前设备OS|

**尚未整机集成或板验。** 当前4个独立工程共7种S31目标构建配置；HFP示例仍含回环，ES8388模拟ADC/DAC不等于CVSD/mSBC软件声码器，通话录音/手机服务/实际刷新/功耗尚未通过。

## 构建

固定ESP-IDF提交 `fff9895c82d744c7237be8847347bdd1b07c6643`（v6.1，S31需`--preview`）。ES8388控制组件锁定`espressif/esp_codec_dev` 1.5.9，源码通过官方组件注册表解析，见`dependencies.lock`。

```sh
bash tools/bootstrap_idf.sh
export IDF_TOOLS_PATH=/tmp/id-card-idf-tools
export IDF_PYTHON_ENV_PATH=/tmp/id-card-idf-python
. /tmp/id-card-esp-idf/export.sh
python3 tools/build_portable.py --project e6 --profile default
```

其他组合：`e6/bringup`、`hfp/classic`、`hfp/dual`、`journal/default`、`audio/default`、`audio/probe`。先用`--print-only`核对命令。新的构建写入`.local-build/`，不依赖原机器的编译缓存。默认硬件探测关闭；不要把构建成功当硬件验收，不要未经项目所有者授权烧录设备。

## 接手与贡献边界

先读技术简报，选择一个可核验工作单元，保存实际源文件/日志与剩余限制，不反复重开芯片泛比较。未发布的产品/成本/结构决策和完整会话应由项目所有者在授权会话中提供；不能凭公开代码推断整机定义已冻结。第三方源码许可与来源见`THIRD_PARTY.md`，本次备份不替所有者新增统一开源许可。
