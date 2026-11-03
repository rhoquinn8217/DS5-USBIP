// FFmpeg's own SBC encoder, kept apart from main.cpp because both encoders have a
// file called sbc.h and the two must not meet in one source file.

#pragma once

#include "sbc_test_signals.h"

#include <cstddef>
#include <cstdint>

struct FfmpegEncoder;

FfmpegEncoder *ffmpeg_encoder_create(const sbctest::Settings &settings);
int ffmpeg_encoder_encode(FfmpegEncoder *encoder, const int16_t *pcm, uint8_t *out, size_t outSize);
void ffmpeg_encoder_destroy(FfmpegEncoder *encoder);
