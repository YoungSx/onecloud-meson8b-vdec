# WanOS V4L2 编解码使用

先按 README 加载 runtime、decoder、encoder 三模块；不改 WanOS 应用层。
ffmpeg 的 v4l2m2m 列表只说明包装器已编译，实际驱动与格式用 QUERYCAP /
ENUM_FMT 验证。`tests/probe-v4l2.c` 可交叉编译后枚举设备；目标机当前没有
v4l2-ctl，也没有 ffprobe，产物在外部主机用 ffprobe 独立检查。

## H.264 硬解到硬编

已验证的 1080p Baseline、单参考帧样片：

```sh
ffmpeg -nostdin -c:v h264_v4l2m2m -num_capture_buffers 7 -i input.mp4 \
  -an -c:v h264_v4l2m2m -b:v 0 -fps_mode passthrough output.mp4
```

`-i` 前的 `-c:v` 指定解码器，后面指定编码器。`-b:v 0` 固定 QP 26；非零
`-b:v` 启用现有软件 CBR。五轮 60 帧 1080p 转码输出都可播放，PSNR 平均
45.41 dB，详见 `evidence/P6-TRANSCODE-ACCEPTANCE-2026-11-14.md`。

缓冲数量必须服从流的 `MIN_BUFFERS_FOR_CAPTURE`。7 只适用于已验证的
Baseline 单参考帧样片。已验证的四参考帧、bframes=2 1080p 输入使用 10 个 CAPTURE 缓冲；
原 64 MiB CMA 下已完成十轮转码，不能把 7 套到该流。失败必须检查日志与输出帧数；
部分 ffmpeg 队列错误可能以成功退出但空输出结束。测试命令用
`timeout -k 5 60`，不会依赖 ffmpeg 自己从死锁中退出。

若需缩图，可在两路间添加 `-vf scale=640:-2`；缩放由 CPU 完成。每次只启动
一个解码与一个编码会话，应用侧排队执行。CPU 字节序转换与输入 staging
仍存在，整个管线未实现零拷贝。

## JPEG

JPEG 通过 encoder 的 `V4L2_PIX_FMT_JPEG` CAPTURE 和
`V4L2_CID_JPEG_COMPRESSION_QUALITY`（1–100）编码；ffmpeg 7.0.2 没有
mjpeg_v4l2m2m 编码包装。可使用仓库 V4L2 验证客户端：

```sh
# 外部交叉构建；复制到目标运行
arm-linux-gnueabihf-gcc-11 -O2 -Wall -Wextra -Werror -static \
  tests/encode-test.c -o build/encode-test
./encode-test /dev/video1 output.mjpg reference.nv12 1920 1080 5 NV12 90 JPEG
```

客户端生成确定性全量程 YCbCr 源，验证时间戳、KEYFRAME、互斥和 EOS。
应用送来的有限量程视频必须先转换到全量程 JPEG YCbCr。输出的 JFIF 头描述
全量程；驱动不会替调用者自动改变样本。

## MPEG 与 OpenList

MPEG-2 硬解可与 H.264 编码搭配，但本轮并发验收集中在 H.264 输入：

```sh
ffmpeg -c:v mpeg2_v4l2m2m -i input.ts -c:v h264_v4l2m2m -b:v 0 -c:a copy output.mp4
```

OpenList 的入库文件可用外部脚本预转码，再让文件服务索引产物。具体应用
集成需遵循当前部署的接口，不用修改 OpenList/WanOS 核心。
此前 OpenList/Arcane 服务验收记录是历史状态；2026-11-14 运行基线中未见
这两个进程/容器，不能认为当前转码链已经接入它们。

## 卸载

结束所有客户端后依次执行 `rmmod meson_venc`、`rmmod meson_vdec`、
`rmmod meson8b_runtime`，恢复被覆盖的固件/overlay 文件。三模块完整卸载已
实测，DOS 资源和 video 节点都撤销，原启动路径保持原状。

## HEVC 与自动加载

HEVC Main 8-bit 可用，1080p B 帧和 854×480 crop 已逐帧匹配。原 ffmpeg
不含 HEVC M2M decoder，用独立包的 tools/ffmpeg（含 V4L2 drain 修复）：

```sh
runtime-package/tools/ffmpeg -c:v hevc_v4l2m2m -num_capture_buffers 8 -i input.mp4 -f null -
```

systemd unit 与可回滚安装/移除脚本见 README。启停和不匹配 overlay 的失败
卸载已实测。OpenList、Arcane 原容器恢复运行并纳入最终服务检查。
