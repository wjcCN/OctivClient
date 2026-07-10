# OctivOutData Mini v1.1

OctivOutData Mini 是 Octiv 工业传感器数据采集工具的轻量版本。它只保留最小采集链路：输入设备 IP、选择频点、选择谐波、选择采样率，然后开始读取传感器 WebService 数据并写出到本地文本文件。

## 功能

- 连接默认地址 `192.168.18.52` 或用户输入的 Octiv 设备 IP。
- 支持 `400kHz`、`2MHz`、`13.56MHz`、`27.12MHz`、`60MHz` 频点选择。
- 支持 0-10 谐波选择。
- 支持 100、200、500、1000 四档采样率配置。
- 实时显示频点、谐波、时间戳、电压和电流。
- 将采集数据写入程序目录下的 `OutData` 文件夹。

## 构建环境

- Windows 10/11
- Visual Studio 2022 或兼容 MSVC 工具链
- Qt 6.11.1 MSVC 2022 64-bit
- CMake 3.16 或更高版本

如果 Qt 安装路径不同，请修改 `CMakePresets.json` 中的 `CMAKE_PREFIX_PATH` 和 `QTDIR`，或设置系统环境变量 `QTDIR`。

## 发行版

当前 mini 版发布在 GitHub Releases：

- [v1.1_mini版本 Release 页面](https://github.com/wjcCN/OctivClient/releases/tag/v1.1_mini%E7%89%88%E6%9C%AC)
- `OctivOutData-v1.1_mini.zip`

