# OneCloud Meson8b V4L2 codecs

为玩客云 S805、WanOS/HiNAS 原版 Linux `5.15.79-meson` 提供运行时加载的
V4L2 Memory-to-Memory 编解码设备。使用原内核的设备树 overlay、canvas、
时钟与媒体框架；构建在外部机器完成，无需改写 `/boot` 或重启。

2026-11-14 实机验证：JPEG 编码八组输入矩阵和 900 帧 1080p 长时检查通过；
H.264 1080p B 帧输入连续十轮硬解到硬编通过，60 帧逐帧 PSNR 最低
45.03 dB、平均 45.82 dB。独立 H.264 硬解与软解 NV12 MD5 60/60 一致；
HEVC Main 8-bit 的小尺寸、裁剪尺寸及 1080p B 帧均逐帧精确匹配。
此前 300 帧硬解、CBR、MPEG-2/MPEG-4/VC-1/MJPEG 验证见 `evidence/`。

## 能力与边界

| 路径 | 当前支持与验证边界 |
| --- | --- |
| 解码 | H.264、MPEG-1/2、MPEG-4 Part 2、VC-1 advanced、MJPEG；各 codec 的帧尾/色度限制见验收记录 |
| H.264 编码 | `ffmpeg -c:v h264_v4l2m2m`，NV12/NV21/YUV420 输入；固定 QP 26 或非零 bitrate 开启软件 CBR；无 B 帧编码 |
| JPEG 编码 | 标准 V4L2 JPEG CAPTURE，质量 1–100；输入为全量程 YCbCr；ffmpeg 7.0.2 没有 JPEG v4l2m2m 编码包装，使用 V4L2 客户端 |
| 并发转码 | 一个解码会话 + 一个编码会话；720p、1080p Baseline 已验证；1080p B 帧并发已通过原 64 MiB CMA 预算验证 |
| HEVC | Main 8-bit 硬解，320×240、854×480 裁剪、1080p B 帧逐帧精确匹配；Main10/tiles 不支持 |

仍有 CPU 字节序转换与编码输入拷贝，不声称端到端零拷贝。900 帧 JPEG、十轮 B 帧并发和重复 HEVC 已验证；任意
参考帧数与超出 Main 8-bit 的 HEVC 内容仍不在验收范围。2026-11-14 服务基线中 SublinkPro、
VoCat、Home Assistant 正常并保持运行；OpenList、Arcane 原容器已恢复运行，
最终五项服务均已验证。

## 架构

- `runtime/meson8b-runtime.ko`：加载挥发性 overlay，统一持有 DOS 寄存器区，
  为共享控制位提供 IRQ 安全更新锁；依赖它的 codec 卸载后才能移除 overlay。
- `vdec/meson-vdec.ko`：staging meson-vdec 的 Meson8b 平台和 codec 移植。
  硬件参考帧保存在独立 DMA shadow，CPU 通过缓存别名转换字节序后输出 NV12M。
- `venc/meson-venc.ko`：HCODEC H.264/JPEG 共用一个 V4L2 设备和单 open 所有权；
  用同一 PMU regmap 原子更新电源位，DMA 区域按厂商布局分开分配。
- `firmware/`：固定 Hardkernel 提交提取的微码、来源和 SHA-256；包括 GPL 许可。

## 外部构建与检查

需要 Linux 5.15.79 OneCloud 匹配源码、目标配置和已准备好的构建目录。
基线来源为 hzyitc/armbian-onecloud `ci-20221118-033748-UTC`，Armbian 提交
`d01e3519a87ab6a3b8df3f71da81d7040137b49f`。本任务外层 `baseline/` 是原发行
内核源码补丁，本仓库 `baseline/` 保留相同副本；外层 `patches/`、研究版
README 和内核包是早期实验存档，
不代表这里已验证的运行时实现。不要安装那个研究版启动包。
从干净源码复现内核构建环境及交付包见 `docs/reproduce.md`。

```sh
# GCC 11 armhf，目标 kernelrelease 必须是 5.15.79-meson
KDIR=/absolute/path/to/matching/kernel-output make modules -j4
python3 scripts/extract-firmware.py --fetch
cc -O2 -Wall -Wextra -Werror tests/test_copy.c -o /tmp/test-copy && /tmp/test-copy
cc -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_jpeg.c -o /tmp/test-jpeg
/tmp/test-jpeg
python3 -m unittest discover -s tests -v
```

目标原配置已启用 `CONFIG_OF_OVERLAY=y`、`CONFIG_MESON_CANVAS=y`、
`CONFIG_V4L2_MEM2MEM_DEV=m`。无需开启原内核的 staging：codec 独立构建。
内核升级后必须按新配置与 ABI 重编，不能强制 insmod 或使用旧 vermagic。

`make dtbo` 的默认 DTS 是本次目标机快照。其他机器必须根据运行中的
`/sys/firmware/devicetree/base` 重新生成，phandle 不可跨设备复用：

```sh
python3 scripts/make-overlay.py --tree /sys/firmware/devicetree/base --output my.dts
dtc -@ -I dts -O dtb -o meson8b-runtime.dtbo my.dts
```

## 加载、使用与回滚

固件和 overlay 放在 `/lib/firmware/meson/{vdec,venc}/`。加载驱动前保留被覆盖
文件副本，模块文件保存在独立目录。模块依赖可由原系统 modprobe 加载：

```sh
modprobe v4l2_mem2mem
modprobe videobuf2_dma_contig
modprobe videobuf2_vmalloc
insmod meson8b-runtime.ko
insmod meson-vdec.ko
insmod meson-venc.ko
# /dev/video 编号按实际注册顺序确定，ffmpeg 自动匹配设备
ffmpeg -c:v h264_v4l2m2m -i input.mp4 -an -c:v h264_v4l2m2m -b:v 0 output.mp4
```

并发时需要按实际流的 `MIN_BUFFERS_FOR_CAPTURE` 调整 ffmpeg 缓冲数量，
不能为省内存把数量降到驱动要求以下。完整命令见 `docs/transcode.md`。

```sh
# 先结束客户端，再移除 codec，最后 runtime；原启动路径未改变
rmmod meson_venc
rmmod meson_vdec
rmmod meson8b_runtime
# 恢复安装前备份的固件/overlay，删除仅本次新增文件
```

完整卸载已验证：codec 节点、DOS 占用和 overlay 都消失，服务保持运行。
`systemd/meson8b-vcodec.service` 提供可回滚自动加载；启停与失败卸载已实测。`scripts/jpeg-matrix.sh`、`scripts/transcode-smoke.sh`
和 `scripts/transcode-stress.py` 是 `/root/venc-deploy` 的实机测试脚本；短时
看门狗护栏和进程 kill deadline 用于 bring-up，不是服务启动机制。

## 验证记录

- `evidence/P6-JPEG-ENCODE-ACCEPTANCE-2026-11-14.md`：JPEG/AVC 回归、固件错误恢复。
- `evidence/P6-TRANSCODE-ACCEPTANCE-2026-11-14.md`：并发修复、五轮压测、CMA 边界。
- `docs/hevc-feasibility.md`：固定源码硬件证据；当前实现验收见 `evidence/HEVC-ACCEPTANCE-2026-11-14.md`。
- `ROADMAP.md`：已完成任务及长期可选项。自动加载已交付；RFC 为本地草稿。
- `docs/rollout-status.md`：最终安装版本、归档校验值、服务状态与回滚命令。

GPL-2.0 / GPL-2.0+，以各文件 SPDX 为准。微码来源许可和提取元数据见
`LICENSE` 与 `firmware/sources.json`。

## 校验包与自动恢复固件

```sh
# 外部主机收集已构建的三个模块、固件和本机专用 overlay
scripts/package-runtime.sh /new/path/runtime-package
# 在目标机执行；state 目录须是新目录，保存原固件副本和每步安装记录
python3 runtime-package/scripts/runtime-device.py check runtime-package
python3 runtime-package/scripts/runtime-device.py apply runtime-package /root/runtime-state
python3 runtime-package/scripts/runtime-device.py rollback /root/runtime-state
```

脚本检查包哈希、原内核版本、OneCloud 身份与运行树 provider phandle；发现
已有 codec 模块时拒绝 apply。固件覆盖前记账并保留副本，加载失败时尝试
卸载后恢复。回滚拒绝覆盖安装后另行更新过的固件，保留 state 目录供核查。
仅修改列入 manifest 的固件文件、加载/卸载模块，不改启动文件。

## 内核源码补丁

`patches/series` 的 additive 补丁将这三个模块加入
`drivers/media/platform/meson/meson8b-vcodec/`，配置入口为
`CONFIG_VIDEO_MESON8B_VCODEC=m`（其依赖限制为模块）。它保留相同的运行时
加载机制；不是静态 DT 启动方案，也没有发布为上游 RFC。

```sh
# 在匹配 OneCloud 5.15.79 基线上，按 series 应用；不叠加旧研究版 vdec 补丁
patch -d /path/to/linux -p1 --fuzz=0 < patches/0001-meson8b-runtime-codecs.patch
# 配置片段：config/runtime-codecs.config
make -C /path/to/kernel-output ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
  CC=arm-linux-gnueabihf-gcc-11 LOCALVERSION=-meson \
  M=/path/to/linux/drivers/media/platform/meson/meson8b-vcodec modules
```

补丁 `--fuzz=0` 应用、新增目录 Kbuild 交叉编译和 Kconfig 选通已核验，日志在
`evidence/kernel-patch-2026-11-14/`。未为此重编或启动新内核；实机使用的是
相同源码的外部模块。源码改动后可用 `scripts/export-kernel-patch.py` 重导出。

## 自动加载与 HEVC 测试工具

```sh
# 原启动内核不变；使用独立包建立可回滚开机加载
sudo bash runtime-package/scripts/install-autoload.sh runtime-package /root/new-codec-state
# 卸载单元、模块并恢复原固件
sudo bash runtime-package/scripts/remove-autoload.sh
```

自动加载启停、平台 probe 失败卸载和完整移除已在目标机测试，未为测试重启。
最终安装保留可用设备；停止该 unit 就会卸载 codec，客户端须先退出。
源码补充包 `build/onecloud-verified-source.tar.gz` 附有独立静态 `tools/v4l2-ctl`，
其设备、格式和控件枚举已在目标机验证；运行包内驱动/固件保持已验收版本。
目标原 ffmpeg 未编入 hevc_v4l2m2m，交付的独立 `tools/ffmpeg` 含该包装器及
V4L2 drain 修复，原 `/usr/local/bin/ffmpeg` 保留。用法：

```sh
runtime-package/tools/ffmpeg -c:v hevc_v4l2m2m -num_capture_buffers 8 -i input.mp4 -f null -
```

该测试工具只启用文件协议及验收所需功能，不能替代完整生产 ffmpeg。
构建复现见 scripts/build-ffmpeg-test.sh；补丁见 patches/ffmpeg-7.0.2-v4l2-drain.patch。
