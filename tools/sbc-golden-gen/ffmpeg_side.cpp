// FFmpeg's own SBC encoder, driven the way the listener drove it until the encoder
// was copied into third_party/sbc: the same calls and the same reach into FFmpeg's
// private frame settings, taken over from src/map/runtime.cpp as it was.

#include "ffmpeg_side.h"

#include <cstddef>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/sbc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

namespace {

struct FfmpegSbcPrivPrefix {
    const AVClass *avClass = nullptr;
    int64_t maxDelay = 0;
    int msbc = 0;
    alignas(SBC_ALIGN) struct sbc_frame frame;
};

}  // namespace

struct FfmpegEncoder {
    AVCodecContext *context = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;
    int samplesPerFrame = 0;
    int sampleRate = 0;
};

FfmpegEncoder *ffmpeg_encoder_create(const sbctest::Settings &settings)
{
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_SBC);
    if (codec == nullptr) {
        return nullptr;
    }
    auto *encoder = new FfmpegEncoder;
    encoder->context = avcodec_alloc_context3(codec);
    AVCodecContext *sbc = encoder->context;
    sbc->sample_rate = static_cast<int>(settings.sampleRate);
    sbc->sample_fmt = AV_SAMPLE_FMT_S16;
    sbc->bit_rate = 225000;
    sbc->global_quality = static_cast<int>(settings.bitpool) * FF_QP2LAMBDA;
    av_channel_layout_default(&sbc->ch_layout, 2);
    if (avcodec_open2(sbc, codec, nullptr) < 0) {
        return nullptr;
    }

    auto *priv = reinterpret_cast<FfmpegSbcPrivPrefix *>(sbc->priv_data);
    priv->frame.frequency = settings.frequency;
    priv->frame.blocks = settings.blocks;
    priv->frame.mode = static_cast<decltype(priv->frame.mode)>(settings.mode);
    priv->frame.channels = 2;
    priv->frame.allocation = static_cast<decltype(priv->frame.allocation)>(settings.allocation);
    priv->frame.subbands = settings.subbands;
    priv->frame.bitpool = settings.bitpool;
    priv->frame.codesize = static_cast<uint16_t>(settings.subbands * settings.blocks * 2 * sizeof(int16_t));
    sbc->frame_size = settings.subbands * settings.blocks;

    encoder->frame = av_frame_alloc();
    encoder->packet = av_packet_alloc();
    encoder->samplesPerFrame = settings.subbands * settings.blocks;
    encoder->sampleRate = static_cast<int>(settings.sampleRate);
    return encoder;
}

int ffmpeg_encoder_encode(FfmpegEncoder *encoder, const int16_t *pcm, uint8_t *out, size_t outSize)
{
    AVFrame *frame = encoder->frame;
    av_frame_unref(frame);
    frame->format = AV_SAMPLE_FMT_S16;
    frame->sample_rate = encoder->sampleRate;
    frame->nb_samples = encoder->samplesPerFrame;
    av_channel_layout_default(&frame->ch_layout, 2);
    if (av_frame_get_buffer(frame, 0) < 0) {
        return -1;
    }
    std::memcpy(frame->data[0], pcm, static_cast<size_t>(encoder->samplesPerFrame) * 2 * sizeof(int16_t));

    int ret = avcodec_send_frame(encoder->context, frame);
    if (ret < 0) {
        return -1;
    }
    size_t written = 0;
    for (;;) {
        ret = avcodec_receive_packet(encoder->context, encoder->packet);
        if (ret == AVERROR(EAGAIN)) {
            break;
        }
        if (ret < 0) {
            return -1;
        }
        const size_t size = static_cast<size_t>(encoder->packet->size);
        if (written + size > outSize) {
            av_packet_unref(encoder->packet);
            return -1;
        }
        std::memcpy(out + written, encoder->packet->data, size);
        written += size;
        av_packet_unref(encoder->packet);
    }
    return static_cast<int>(written);
}

void ffmpeg_encoder_destroy(FfmpegEncoder *encoder)
{
    av_packet_free(&encoder->packet);
    av_frame_free(&encoder->frame);
    avcodec_free_context(&encoder->context);
    delete encoder;
}
