# OctivClient v1.1

OctivClient 是一个面向 Octiv 工业传感器的 Qt 6 桌面客户端，用于通过 WebService 接口连接传感器、读取设备信息、配置采样参数，并实时查看五通道测量数据与温度状态。

![OctivClient v1.1 界面](docs/images/octiv-client-v1.1-ui.png)

## 主要功能

- 通过 IP 地址连接 Octiv 工业传感器 WebService。
- 读取序列号、传感器类型、固件版本、FPGA 版本、校准日期等设备信息。
- 配置刷新周期、CH1-CH5 谐波参数和信号锁定模式。
- 轮询 PCB 温度、传感器温度以及五通道实时数据。
- 展示频率、电压、电流、相位等实时测量字段。
- 提供中英文界面切换。
- 输出通信日志，便于现场调试和问题定位。

## 系统结构

![OctivClient 系统结构](docs/images/octiv-client-v1.1-architecture.png)

客户端通过 Qt Network 发起 HTTP/WebService 请求，设备返回 JSON 数据后由解析模块转换为界面数据模型，再刷新主窗口表格、状态区和日志区。

## 使用流程

![OctivClient 工作流程](docs/images/octiv-client-v1.1-workflow.png)

1. 输入传感器 IP 地址，点击“连接”。
2. 点击“获取信息”读取设备基本信息。
3. 根据需要读取或应用设备配置。
4. 点击“开始数据”进入实时数据采集。
5. 在表格和通信日志中查看数据状态。

## 构建环境

- Windows 10/11
- Visual Studio 2022 或兼容 MSVC 工具链
- Qt 6.11.1 MSVC 2022 64-bit
- CMake 3.16 或更高版本

本仓库已包含 `CMakePresets.json`，默认使用本机路径：

```text
D:/Programs/Qt/6.11.1/msvc2022_64
```

如果 Qt 安装在其他目录，可修改 `CMakePresets.json` 中的 `CMAKE_PREFIX_PATH` 和 `QTDIR`，或在系统环境变量中设置 `QTDIR`。

## 发行版

当前发布包已发布到 GitHub Releases：

- [v1.1版本 Release 页面](https://github.com/wjcCN/OctivClient/releases/tag/v1.1%E7%89%88%E6%9C%AC)
- [OctivClient-v1.1.zip 直接下载](https://github.com/wjcCN/OctivClient/releases/download/v1.1%E7%89%88%E6%9C%AC/OctivClient-v1.1.zip)

压缩包内包含可执行程序和 Qt 运行时依赖，可在目标 Windows 机器上解压后运行 `OctivClient.exe`。发行包不再作为普通源码文件存放在仓库中，源码仓库只保留源代码、构建配置和项目文档。
