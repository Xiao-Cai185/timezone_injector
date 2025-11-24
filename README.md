# 时区注入器 (Timezone Injector)

一个 Windows GUI 应用程序，可以将自定义时区设置注入到任何可执行进程中。

## 🎯 功能特性

- **图形界面**：易于使用的 Windows GUI，可选择目标程序和时区
- **多时区支持**：预配置了主要时区（UTC、东京、北京、纽约等共20个时区）
- **进程注入**：自动将时区钩子 DLL 注入到目标进程
- **无外部依赖**：完全自包含，无需 Detours 或其他第三方库
- **自动构建**：GitHub Actions 自动构建并提交二进制文件

## 📦 下载使用

预编译的二进制文件位于 `build/` 目录：
- `gui_injector.exe` - 主 GUI 应用程序
- `timezonehook.dll` - 时区钩子 DLL（GUI 自动使用）
- `injector.exe` - 传统命令行注入工具

## 🚀 使用方法

### GUI 方式（推荐）

1. 运行 `gui_injector.exe`
2. 点击"Browse..."选择目标可执行文件
3. 从下拉菜单选择所需时区
4. 点击"Launch & Inject"

目标应用程序将以自定义时区设置启动。

### 命令行方式

```batch
injector.exe <目标程序.exe> <timezonehook.dll的路径>
```

**注意**：命令行方式需要手动通过环境变量配置时区：
- `TZ_BIAS`：时区偏移（分钟），例如 -540 表示 UTC+9
- `TZ_NAME`：时区显示名称

示例：
```batch
set TZ_BIAS=-540
set TZ_NAME=Tokyo (UTC+9)
injector.exe notepad.exe timezonehook.dll
```

## 🏗️ 从源码构建

### 前置要求

- Windows 操作系统
- Visual Studio 2019 或更高版本（带 MSVC 编译器）
- Git

### 本地构建

```batch
# 设置 MSVC 环境（根据您的 VS 安装路径调整）
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

# 创建构建目录
mkdir build

# 构建 GUI 应用程序
cl /O2 /EHsc /W3 gui_injector.cpp /link /SUBSYSTEM:WINDOWS /out:build/gui_injector.exe user32.lib gdi32.lib kernel32.lib comdlg32.lib

# 构建钩子 DLL
cl /LD /O2 /EHsc /W3 timezonehook.cpp /link /out:build/timezonehook.dll kernel32.lib

# 构建命令行注入器（可选）
cl /O2 /EHsc /W3 injector.cpp /link /out:build/injector.exe kernel32.lib
```

### GitHub Actions 自动构建

项目使用 GitHub Actions 进行自动构建。每次推送到 `main` 或 `master` 分支时：

1. 使用 MSVC 编译代码
2. 在 `build/` 目录生成二进制文件
3. 自动将构建产物提交回仓库
4. 同时上传为 GitHub Actions 构建产物

## 🔧 工作原理

### 架构说明

1. **GUI 应用程序** (`gui_injector.cpp`)：
   - 提供用户界面选择目标程序和时区
   - 设置环境变量（`TZ_BIAS`、`TZ_NAME`）
   - 以挂起状态创建目标进程
   - 将 `timezonehook.dll` 注入到目标进程
   - 恢复目标进程执行

2. **钩子 DLL** (`timezonehook.cpp`)：
   - 从环境变量读取时区配置
   - 使用 IAT（导入地址表）钩子技术钩住 Windows 时区 API 函数
   - 拦截 `GetTimeZoneInformation` 和 `GetDynamicTimeZoneInformation`
   - 向应用程序返回自定义时区信息

### 支持的时区

| 时区 | UTC 偏移 | Bias（分钟） |
|------|---------|-------------|
| UTC | +0 | 0 |
| 东京 | +9 | -540 |
| 首尔 | +9 | -540 |
| 北京/上海 | +8 | -480 |
| 香港 | +8 | -480 |
| 新加坡 | +8 | -480 |
| 曼谷 | +7 | -420 |
| 孟买 | +5:30 | -330 |
| 迪拜 | +4 | -240 |
| 莫斯科 | +3 | -180 |
| 伊斯坦布尔 | +3 | -180 |
| 巴黎/柏林 | +1 | -60 |
| 伦敦 | +0 | 0 |
| 纽约 | -5 | 300 |
| 芝加哥 | -6 | 360 |
| 丹佛 | -7 | 420 |
| 洛杉矶 | -8 | 480 |
| 悉尼 | +10 | -600 |
| 墨尔本 | +10 | -600 |
| 奥克兰 | +12 | -720 |

**注意**：Bias 值对于 UTC 以东的时区为负数，UTC 以西的时区为正数。

## 📝 技术细节

### IAT 钩子技术

DLL 使用导入地址表（IAT）钩子而不是内联钩子或 Detours：
- **优点**：无外部依赖、实现简单、适用于大多数应用程序
- **限制**：仅钩住目标可执行文件 IAT 中导入的函数

### 环境变量

时区配置通过环境变量传递：
- `TZ_BIAS`：相对于 UTC 的分钟偏移（东为负，西为正）
- `TZ_NAME`：时区显示名称

### 进程注入流程

注入过程：
1. 以挂起状态创建目标进程
2. 在目标进程中分配内存
3. 将 DLL 路径写入分配的内存
4. 创建远程线程调用 `LoadLibraryW`
5. 等待 DLL 加载完成
6. 恢复主线程

## ⚠️ 限制说明

- 仅适用于 Windows 系统
- 目标应用程序必须从 kernel32.dll 导入时区函数
- 某些应用程序可能使用其他方法获取时区信息
- 需要适当的权限才能注入进程

## 🛠️ 故障排除

**"Failed to create process"（创建进程失败）**
- 确保目标可执行文件路径正确
- 检查是否有运行该可执行文件的权限
- 尝试以管理员身份运行

**"Failed to create remote thread"（创建远程线程失败）**
- 某些受保护的进程无法注入
- 尝试以管理员身份运行
- 检查杀毒软件是否拦截

**时区未改变**
- 目标应用程序可能缓存了时区信息
- 某些应用程序使用此工具未钩住的其他时区 API
- 尝试重启应用程序

**"DLL file not found"（找不到 DLL 文件）**
- 确保 `timezonehook.dll` 与 `gui_injector.exe` 在同一目录
- 检查错误消息中显示的预期 DLL 路径

## 📄 许可证

详见 [LICENSE](LICENSE) 文件。

## 🤝 贡献

欢迎贡献！请随时提交问题或拉取请求。

## 📧 支持

如有问题和疑问，请使用 GitHub Issues 页面。

## 🔄 更新日志

### 主要改进

- ✅ 移除了 Detours 外部依赖
- ✅ 实现自定义 IAT 钩子
- ✅ 扩展时区列表至 20 个
- ✅ 增强错误处理和用户反馈
- ✅ 添加 DLL 存在性检查
- ✅ 完善的 GitHub Actions 自动构建
- ✅ 完整的中文文档

## 💡 使用场景

### Web 开发
测试 Web 应用在不同时区下的行为：
```
选择浏览器 → 选择时区 → 测试您的 Web 应用
```

### 游戏测试
访问时区锁定的游戏内容：
```
选择游戏 → 选择时区 → 启动游戏
```

### 软件质量保证
验证应用程序的时区处理：
```
选择应用 → 测试所有时区 → 发现 Bug
```

## 🔐 安全说明

### 杀毒软件误报

某些杀毒软件可能会标记此工具，因为：
- 使用了 DLL 注入技术
- 修改进程内存
- 钩住 API 函数

这是**误报**。工具不包含恶意代码，源代码完全开放可审查。

### 管理员权限

在以下情况下可能需要以管理员身份运行：
- 目标应用程序需要提升权限
- 遇到"访问被拒绝"错误
- 注入失败并显示错误代码 5
