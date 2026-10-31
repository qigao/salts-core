# SaltsUtils Capture

`Salts::Capture` 是 Windows 默认提供、其他平台可选的原生音频、摄像头与屏幕
采集组件。它保留
`salts_capture.h` 的 C 接口、枚举值、结构布局和符号名称；播放、编解码、录制、
RTSP、WebRTC 以及移动端 Java/Swift 包装不属于该组件。

## 构建与链接

Windows 配置始终启用该组件，标准 Windows preset 也统一选择 vcpkg 的 `capture`
feature。其他平台的裸 CMake 配置默认关闭；显式启用时同时选择该 feature：

```sh
cmake -S . -B build/capture \
  -DSALTS_ENABLE_CAPTURE=ON \
  -DVCPKG_MANIFEST_FEATURES=capture
cmake --build build/capture
```

所有标准 Windows preset 都启用 Capture；构建其常规 `install` target 时会和其他库一起
安装并导出 `Salts::Capture`，无需 Capture 或 Release 专用 install preset。既有
Windows capture preset 仅为兼容已有命令保留；Linux 仍使用对应的 capture-enabled
preset。安装后，消费端只依赖导出的组件：

```cmake
find_package(SaltsUtils CONFIG REQUIRED)
target_link_libraries(app PRIVATE Salts::Capture)
```

Windows 使用 Media Foundation、DirectShow、D3D11 与 DXGI；Linux 配置时要求
PipeWire 0.3、X11 和 Xext；macOS/iOS 使用系统 Framework；Android 使用 NDK
Camera2、Media、OpenSL ES 和 NativeWindow。miniaudio 与 libyuv 是实现依赖，其类型
不会进入公开头文件。Windows 动态构建会把 `libyuv.dll` 及其 `jpeg62.dll` 运行时依赖
安装到 SDK 的 `bin/`，与 `salts_capture.dll` 一起部署即可满足加载依赖。

## 所有权、线程与关闭

- capture 实例拥有原生句柄和格式转换缓冲区。
- 音频块或视频帧是只读 borrowed view，只在同步 callback 返回前有效。需要保留数据
  的调用方必须在 callback 内复制。
- view 不得跨 callback 返回、`stop`、`destroy` 或协程挂起保存。
- 每个实例只有一个原生 producer 路径；组件不增加队列、fan-out 或无界缓存。慢
  callback 可能增加延迟或使原生后端丢帧。
- create/start/stop/destroy、callback setter 和设备控制属于 control plane；同一实例
  需要由调用方串行化，不能从该实例自己的 callback 中调用 stop/destroy。
- 正常关闭顺序是先 `salts_capture_stop()`，等待后端停止投递，再调用
  `salts_capture_destroy()`。启动失败进入 `SALTS_CAPTURE_STATE_ERROR`，不会报告
  `RUNNING`。

设备不存在时，枚举返回零项。参数、权限、格式协商、原生资源或模式身份无效时接口会
明确失败，不会静默切换为另一种设备语义。

### 音频输入合同（4.1.12）

- `salts_audio_capture_create(device_id, ...)` 的非空 `device_id` 必须来自当前音频设备枚举并精确匹配；未知、失效或后端不支持的显式 ID 直接失败，不会回退默认麦克风。NULL/空字符串才表示默认设备。
- 音频格式不做隐式转换。Desktop 接受公开的 16/32-bit integer PCM；Android 当前明确支持 S16；iOS 按请求配置 Int16/Int32。
- `frame_size_ms` 是 producer period 合同。iOS 的 preferred I/O buffer duration 与 AVAudioEngine tap frame count 均按 `sample_rate * frame_size_ms / 1000` 请求，不再使用固定 5 ms / 1024 frames。
- callback payload 仍是 borrowed view。若底层平台实际回调长度与请求 period 不一致，消费端应显式失败或自行采用有界转换层，不得截断、拼接或静默改变帧语义。

Android MediaProjection 使用 `<salts_capture_android.h>`。Salts 拥有
`salts_capture_t`、`AImageReader` 与其 `ANativeWindow`；Java 拥有权限结果、
`MediaProjection`、`VirtualDisplay` 和从 JNI 返回的 `Surface` 对象。控制顺序必须是：
创建 capture、取得 Surface、创建 VirtualDisplay、启动 capture；关闭时先释放
VirtualDisplay/MediaProjection，再 stop/destroy capture。该接口直接使用 API 26 的
`ANativeWindow_toSurface()`，所以 Android 最低平台是 API 26，不提供低版本 fallback。
ImageReader listener 仅在 capture 为 RUNNING 时交付并计数帧；`stop` 会关闭该 gate 并
等待已进入的 callback 返回，因此 `stop` 返回后不会继续访问 callback payload。

## 下游迁移

TurboMedia 应在单独的改动中将 `TurboMedia::Device` 调用方切换到已安装的
`Salts::Capture`，运行其设备与打包测试后，再删除 TurboMedia 中重复的 capture
源码。该顺序使回滚只需恢复链接目标，不需要恢复已删除的实现文件。

Windows 和 Android arm64 可由本仓库 presets 在当前环境编译验证。Linux、macOS 与 iOS
仍应在对应原生 runner 上完成 configure、link、生命周期和有硬件/权限条件的设备测试。
