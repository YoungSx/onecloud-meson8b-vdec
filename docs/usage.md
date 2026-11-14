# 编解码使用

先按[安装指南](installation.md)加载设备。编号按注册顺序分配；原验收
设备的 decoder 为 `/dev/video0`，encoder 为 `/dev/video1`。

## 设备枚举

```sh
/opt/meson8b-vcodec/current/tools/v4l2-ctl --list-devices
/opt/meson8b-vcodec/current/tools/v4l2-ctl -d /dev/video0 --list-formats-out --list-formats --list-ctrls
/opt/meson8b-vcodec/current/tools/v4l2-ctl -d /dev/video1 --list-formats-out --list-formats --list-ctrls
```

枚举只证明 ioctl 与格式暴露，完整帧数/内容才是解码验收。FFmpeg 的
v4l2m2m 列表也仅证明包装器存在。

## H.264 硬解到硬编

使用目标原生产 FFmpeg。已验收的 Baseline 单参考帧样片用 7 个 CAPTURE
缓冲；四参考帧、bframes=2 的 1080p 输入使用 10 个：

```sh
ffmpeg -nostdin -c:v h264_v4l2m2m -num_capture_buffers 10 -i input.mp4 \
  -an -c:v h264_v4l2m2m -b:v 0 -fps_mode passthrough output.mp4
```

`-i` 前选 decoder，后选 encoder。`-b:v 0` 使用固定 QP 26；非零 bitrate
启用软件 CBR。缓冲数量必须至少为实际流的 `MIN_BUFFERS_FOR_CAPTURE`，
10 也不是任意流的通用常量。原 64 MiB CMA 下十轮 60 帧 B 帧输入完整输出，
逐帧 PSNR 最低 45.03 dB、平均 45.82 dB，见[验收](validation.md)。

每次一个 decoder 和一个 encoder，应用侧排队。`-vf scale=640:-2` 可缩图，
缩放由 CPU 完成；数据路径仍有字节序转换和输入拷贝。

## HEVC Main 8-bit

目标原 FFmpeg 未编入 `hevc_v4l2m2m`，使用运行包的独立工具：

```sh
/opt/meson8b-vcodec/current/tools/ffmpeg -nostdin \
  -c:v hevc_v4l2m2m -num_capture_buffers 8 -i input.mp4 -f null -
```

该工具只启用文件协议和验收所需功能；不能替代完整生产 FFmpeg。
支持 Main 8-bit 线性输出，Main10、tiles、长期参考帧不支持。

## JPEG 编码

V4L2 CAPTURE 为 `V4L2_PIX_FMT_JPEG`，质量控件 1–100。FFmpeg 7.0.2 没有
JPEG M2M encoder 包装，使用仓库客户端，先在外部交叉构建：

```sh
mkdir -p build
arm-linux-gnueabihf-gcc-11 -O2 -Wall -Wextra -Werror -static \
  tests/encode-test.c -o build/encode-test
# 复制到目标机后
./encode-test /dev/video1 output.mjpg reference.nv12 1920 1080 5 NV12 90 JPEG
```

输入是全量程 YCbCr，客户端生成确定性图像并检查时间戳、KEYFRAME、互斥、
LAST/EOS。有限量程源须先转换；驱动不自动改变样本范围。长测试可把原始
参考输出设为 `/dev/null`，避免填满设备 `/tmp` 的 tmpfs。

## 其他格式与应用

MPEG/VC-1/MJPEG 的输出及帧尾边界见[验证指南](validation.md)。MPEG-2
输入到 H.264 编码可按以下方式调用，当前并发验收集中在 H.264 输入：

```sh
ffmpeg -c:v mpeg2_v4l2m2m -i input.ts -c:v h264_v4l2m2m -b:v 0 -c:a copy output.mp4
```

OpenList 可索引预转码后的文件；应用内部自动转码链未配置，不修改
OpenList/WanOS 核心。故障、空输出或卡住按[排查指南](troubleshooting.md)
核查，不用成功退出码替代完整帧数与内容检查。
