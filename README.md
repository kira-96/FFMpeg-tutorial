# FFmpeg-tutorial

目前基于 [FFmpeg](https://ffmpeg.org/) *n4.3.1* 版本。

准备工作：

- Visual Studio 2019
- [Download FFmpeg packages](https://ffmpeg.org/download.html#build-windows)
- [Download SDL2](http://www.libsdl.org/download-2.0.php) 用于播放

注意：

1. 下载 FFmpeg 时请选择 `LGPL` 或 `shared` 标记的二进制包
2. 所有项目均使用 **x64** (64位)

开发环境：

解压缩你下载的 FFmpeg 二进制包，将以下文件夹复制到 `examples` 文件夹下。

| 文件夹名称 | 目标文件夹 |
| :-------: | :-------: |
|   bin  | examples/bin |
| include | examples/include |
|   lib  | examples/lib |

解压缩你下载的 SDL2 二进制包，将一下文件（夹）复制到 `examples` 文件夹下。

| 文件（夹）名称 | 目标文件夹 |
| :-----------: | :-------: |
|    include   | examples/include/libsdl |
| lib/x64/SDL2.dll | examples/bin/SDL2.dll |
|    lib/x64   | examples/lib |

编译生成的可执行程序放在 `examples/bin` 文件夹下。
