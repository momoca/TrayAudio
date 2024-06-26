# TrayAudio

![功能介绍](video.gif)

这个应用程序是一个基于 Windows 的系统托盘应用，用于显示动态音频波形图标。应用程序的主要功能包括：

1. **音频捕获**：使用 Windows 音频 API (`IAudioClient` 和 `IAudioCaptureClient`) 捕获系统音频输出，并对音频数据进行处理。
2. **动态图标**：根据捕获的音频数据生成动态更新的托盘图标，实时显示音频波形。
3. **托盘图标管理**：创建和管理系统托盘图标，包括添加图标、更新图标和删除图标。
4. **右键菜单**：为托盘图标添加右键菜单，提供退出选项，允许用户通过右键菜单关闭应用程序。
5. **图形绘制**：使用 GDI+ 绘制音频波形，并生成动态图标。

应用程序在后台运行，没有可见窗口，通过托盘图标与用户进行交互。用户可以右键点击托盘图标并选择“退出”来关闭应用程序。应用程序还处理系统音频数据，并将其转换为波形图，显示在托盘图标中，实现了一个实时动态的视觉效果。
