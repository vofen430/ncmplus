# ncmplus

使用本程序可将下载的网易云音乐缓存文件（`.ncm`）转换为 `mp3` 或 `wav` 格式；对于无损文件，如果你希望保留原始 `flac` 输出，也可以使用 `-f` 参数。

## 简介

该版本为最早的 C++ 版本，也是作者开发的市面上第一个支持 ncm 转换的程序。

源码folk自 `taurusxin/ncmdump`。原始代码来自`taurusxin/ncmdump`，虽然原仓库已经删除，但最初的实现为后续项目打下了基础，在此感谢原库作者的开源贡献与技术积累。

此版本仅做了一些易用性调整，按个人习惯无损音乐默认转码为wav。

## 使用

注意：网易云音乐 3.0 之后的某些版本，下载得到的 `.ncm` 文件可能不再内置歌曲专辑封面，所需封面图数据需要从网络获取。考虑到在一个小工具中嵌入大型网络库并无必要，如果你需要更完整的封面处理能力，可以移步作者的其他实现或基于该思路开发的 GUI 工具。

### 命令行工具

使用 `-h` 或 `--help` 参数来打印帮助：

```shell
ncmplus -h
```

使用 `-v` 或 `--version` 参数来打印版本信息：

```shell
ncmplus -v
```

处理单个或多个文件：

```shell
ncmplus 1.ncm 2.ncm...
```

使用 `-d` 参数来指定一个文件夹，对文件夹下的所有以 `.ncm` 为扩展名的文件进行批量处理：

```shell
ncmplus -d source_dir
```

使用 `-r` 配合 `-d` 参数来递归处理文件夹下的所有以 `.ncm` 为扩展名的文件：

```shell
ncmplus -d source_dir -r
```

默认输出目录为 `./output`，你也可以使用 `-o` 参数来指定输出目录；当与 `-r` 一起使用时，会保留源目录结构：

```shell
# 处理单个或多个文件并输出到指定目录
ncmplus 1.ncm 2.ncm -o output_dir

# 处理文件夹下的所有 .ncm 文件并输出到指定目录，不包含子文件夹
ncmplus -d source_dir -o output_dir

# 递归处理文件夹并输出到指定目录，同时保留目录结构
ncmplus -d source_dir -o output_dir -r
```

对于无损类型的 `.ncm`，程序会先解密得到 `flac`，然后默认自动继续转换为 `wav`。如果你想保留 `flac` 输出，可以使用 `-f`：

```shell
# 无损文件默认输出 wav
ncmplus 1.ncm

# 保留无损 flac 输出
ncmplus 1.ncm -f
```

说明：默认无损转 `wav` 依赖系统中的 `ffmpeg`。如果未安装 `ffmpeg`，程序会给出警告并保留 `flac` 输出。

使用 `--remove` 参数时，不再删除文件，而是将受支持的音乐文件（`.ncm`、`.wav`、`.flac`、`.mp3`）以二进制方式清空为 0 字节文件。

对于 `.ncm` 文件，会在成功处理后再清空；对于其余三种格式，会直接清空文件内容。`-r` 与 `-d` 一起使用时，上述逻辑同样会递归生效：

```shell
ncmplus 1.ncm --remove

# 递归处理并在成功后清空源 .ncm 内容
ncmplus -d source_dir -r --remove
```

当使用 `-d` 扫描目录而当前层没有可处理文件时，程序会输出明确提示；使用 `-r` 时也会自动跳过输出目录，避免递归扫描到自己生成的结果。

### 动态库

如果你想利用此项目进行二次开发，例如在 C#、Python、Java 等项目中调用，也可以使用 `libncmdump` 动态库。相关示例见仓库中的 `example` 目录。

请注意：如果你在 Windows 下开发，传递到库构造函数的文件名编码**必须为 UTF-8**，否则可能抛出运行时错误。

## 编译项目

克隆本仓库：

```shell
git clone https://github.com/vofen430/ncmplus.git
cd ncmplus
```

### 依赖说明

- `TagLib`：用于写入标题、歌手、专辑、封面等元数据。
- `ffmpeg`：仅在“无损文件默认转 `wav`”这一路径下需要；如果不可用，程序会回退为保留 `flac`。

其中 `TagLib` 不是强制依赖；若构建时未找到，程序仍可编译，但会禁用元数据写入功能。

### Windows

安装 Visual Studio 2022 和 CMake，并准备好 C++ 桌面开发环境；如需完整元数据写入，建议使用 `vcpkg` 安装 `taglib`。

```shell
# 安装 vcpkg 并安装 taglib
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
vcpkg install taglib:x64-windows-static
```

配置并编译项目：

```shell
cmake -G "Visual Studio 17 2022" ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -B build

cmake --build build -j 8 --config Release
```

### macOS

macOS 下可以方便地使用 Homebrew 来安装 `taglib`：

```shell
brew install taglib

cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

### Linux

Linux 下请先安装 `taglib` 开发库；如果系统仓库版本过旧，也可以自行编译较新的版本。随后使用 CMake 配置并编译项目：

```shell
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build -j$(nproc)
```

---

你可以在构建目录下找到编译好的二进制文件；在 Windows 下还会同时生成一个可供其他项目使用的动态库。具体调用方式见仓库中的 `example` 目录。

## 致谢

感谢 `anonymous5l/ncmdump` 与 `taurusxin/ncmdump` 作者的开源贡献。正是前人的探索与实现，才使得后续版本得以在兼容性、构建方式和使用体验上继续向前推进。

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=vofen430/ncmplus&type=Date)](https://star-history.com/#vofen430/ncmplus&Date)
