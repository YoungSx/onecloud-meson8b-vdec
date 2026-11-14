# 复现已验证的运行时交付

目标为原版 OneCloud Linux 5.15.79-meson。交叉构建在外部 Ubuntu 24.04
x86_64/ARM64 主机进行。需要 GCC/G++ 11 armhf、GCC 11 plugin headers、
make、flex、bison、bc、libssl-dev、libelf-dev、dtc、kmod、curl、patch。
下列步骤不向目标机写入新内核或启动设备树。
使用补充包时，先把 `sources/`、`supplement/scripts/`、`supplement/baseline/`
复制到 `payload-source/`，在该目录执行命令。保留的 `SOURCE_COMMIT` 使归档
源码也能生成来源明确的运行包，无需建立新的 Git 提交。

## 内核与模块

仓库 `baseline/0001-armbian-onecloud-5.15.79.patch` 的 SHA-256 为
`3d2b062d672124b798fd3379fd53a28ce8563e86528f9643306d6cfde415d33f`，
重建 hzyitc/armbian-onecloud ci-20221118-033748-UTC 的源码线。原配置在
`config/target-5.15.79-meson.config`。从 Linux 5.15.79 官方 tarball 开始：
源码补充包的 `sources/linux-5.15.79.tar.xz` 可直接复制到当前目录，省去下载。

```sh
test -f linux-5.15.79.tar.xz || curl -fL https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.79.tar.xz -o linux-5.15.79.tar.xz
echo 'cba39031dbc0eed0785b8afdc8c58cf23df83e47001b2354fa44486ae699c154  linux-5.15.79.tar.xz' | sha256sum -c -
tar -xf linux-5.15.79.tar.xz
patch -d linux-5.15.79 -p1 --fuzz=0 < baseline/0001-armbian-onecloud-5.15.79.patch
cp config/target-5.15.79-meson.config linux-5.15.79/.config
make -C linux-5.15.79 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
  CC=arm-linux-gnueabihf-gcc-11 LOCALVERSION=-meson olddefconfig
make -C linux-5.15.79 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
  CC=arm-linux-gnueabihf-gcc-11 LOCALVERSION=-meson -j4 vmlinux modules
make -C linux-5.15.79 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
  CC=arm-linux-gnueabihf-gcc-11 LOCALVERSION=-meson kernelrelease
# 必须输出 5.15.79-meson；完整构建生成 Module.symvers 供 MODPOST 检查
KDIR="$PWD/linux-5.15.79" make modules -j4
```

需要把 codec 集成到源码树时，在相同基线应用 `patches/series` 中唯一的
`0001-meson8b-runtime-codecs.patch`，合并 `config/runtime-codecs.config`。
该补丁模块目录的 Kbuild 已交叉编译通过。保留的其他旧平台研究补丁不属于
当前 series。原内核已提供所需媒体/时钟/canvas 符号，不需要替换启动内核。
仅 `modules_prepare` 不生成 `Module.symvers`，会失去导出符号和模块依赖
检查；不要把这类 unresolved-symbol 警告当作完整构建成功。

## Overlay、固件与独立工具

默认 `meson8b-runtime.dts` 仅适用于本次运行设备树。更换设备或设备树后，
在目标机读取当前 provider phandle 重新生成 DTS，再到构建主机编译 dtbo。
安装器会检查目标身份和 phandle。固定厂商微码及提取元数据位于 `firmware/`。

```sh
make dtbo
python3 scripts/extract-firmware.py --fetch
bash scripts/build-ffmpeg-test.sh
bash scripts/build-v4l2-ctl.sh
bash scripts/package-runtime.sh build/new-runtime-package
```

FFmpeg 7.0.2 来源及 SHA-256 固定在构建脚本中；独立工具只启用文件协议
和测试功能，不替代目标机原生产 FFmpeg。v4l2-ctl 1.22.1 独立静态链接，
不使用 libv4l 包装，可直接枚举 M2M ioctl。

## 本次产物与验证范围

`build/onecloud-verified-release.tar.gz` 是最终实机安装的运行包，来源提交
`bd1cb79ad502d0b8cdcf9667a0ceb0ca0f7876ab`。其 SHA-256 和模块/独立工具
哈希在 `evidence/hevc-2026-11-14/artifact-hashes.json`。随附源码补充包包含
该提交的驱动、构建脚本、微码源与原 Linux/FFmpeg/v4l-utils tarball，便于离线重建。
后续文档、枚举工具与证据提交没有改变已安装的驱动/固件。

构建、模块安装、framehash 内容、错误恢复、systemd 启停和完整卸载均有
日志；未重启、新启动内核与当前主线端口未验证。Linux-media RFC 为本地
草稿。原始启动文件保留，回滚恢复安装前固件快照并撤销三个模块及 unit。
