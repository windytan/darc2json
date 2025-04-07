/*
 * Copyright (c) Oona Räisänen
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
#include "src/layer1.h"

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <iostream>
#include <vector>

#include <sndfile.h>

#include "src/common.h"
#include "src/input.h"
#include "src/liquid_wrappers.h"

namespace darc2json {

namespace {

constexpr float kCarrierFrequency_Hz = 76'000.0f;
constexpr float kBitsPerSecond       = 16'000.0f;
constexpr int kSamplesPerSymbol      = 8;
constexpr float kAGCBandwidth_Hz     = 500.0f;
constexpr float kAGCInitialGain      = 0.0077f;
constexpr float kLowpassCutoff_Hz    = 11'000.0f;
constexpr float kPLLBandwidth_Hz     = 0.01f;

constexpr float hertz2step(float Hz) {
  return Hz * 2.0f * M_PI / kTargetSampleRate_Hz;
}

}  // namespace

Subcarrier::Subcarrier(const Options& options)
    : sample_num_(0),
      resample_ratio_(kTargetSampleRate_Hz / options.samplerate),
      bit_buffer_(),
      fir_lpf_(64, kLowpassCutoff_Hz / kTargetSampleRate_Hz),
      agc_(kAGCBandwidth_Hz / kTargetSampleRate_Hz, kAGCInitialGain),
      oscillator_subcarrier_(LIQUID_VCO, hertz2step(kCarrierFrequency_Hz)),
      oscillator_dataclock_(LIQUID_VCO, hertz2step(kBitsPerSecond / 2)),
      resampler_(resample_ratio_, 13),
      freqdem_(0.5f),
      is_eof_(false),
      accumulator_(0.f) {
  oscillator_dataclock_.setPLLBandwidth(kPLLBandwidth_Hz / kTargetSampleRate_Hz);

  if (options.input_type == InputType::MpxSndfile) {
    mpx_ = new SndfileReader(options);
  } else {
    mpx_ = new StdinReader(options);
  }

  resample_ratio_ = kTargetSampleRate_Hz / mpx_->samplerate();

  if (resample_ratio_ >= 4.f || resample_ratio_ <= 0.004f) {
    throw std::runtime_error("error: sample rate is out of range");
  }

  resampler_.set_rate(resample_ratio_);

  // TODO: The PLL is not used.
  oscillator_subcarrier_.setPLLBandwidth(kBitsPerSecond * 0.1f / nyquist() * 2.f * 0.01f * 0.2f);
}

float Subcarrier::nyquist() const {
  return kTargetSampleRate_Hz * .5f;
}

void Subcarrier::DemodulateMoreBits() {
  is_eof_ = mpx_->eof();
  if (is_eof_)
    return;

  const std::vector<float> inbuffer = mpx_->ReadChunk();

  int num_samples = 0;

  std::vector<std::complex<float>> complex_samples(
      resample_ratio_ <= 1.0f ? inbuffer.size() : std::ceil(inbuffer.size() * resample_ratio_));

  if (resample_ratio_ == 1.0f) {
    for (std::size_t i = 0; i < inbuffer.size(); i++) complex_samples[i] = inbuffer[i];
    num_samples = inbuffer.size();
  } else {
    int i_resampled = 0;
    for (std::size_t i = 0; i < inbuffer.size(); i++) {
      std::array<std::complex<float>, 4> buf;
      int num_resampled = resampler_.execute(inbuffer[i], buf.data());

      for (int j = 0; j < num_resampled; j++) {
        complex_samples[i_resampled] = buf[j];
        i_resampled++;
      }
    }
    num_samples = i_resampled;
  }

  constexpr int kDecimateRatio = kTargetSampleRate_Hz / kBitsPerSecond / kSamplesPerSymbol;

  for (int i = 0; i < num_samples; i++) {
    const std::complex<float> sample          = complex_samples[i];
    const std::complex<float> sample_baseband = oscillator_subcarrier_.MixDown(sample);

    fir_lpf_.push(sample_baseband);

    if (sample_num_ % kDecimateRatio == 0) {
      const std::complex<float> sample_lopass = agc_.execute(fir_lpf_.execute());

      const float fmdem = freqdem_.Demodulate(sample_lopass);

      accumulator_ += fmdem * std::fabs(oscillator_dataclock_.cos());
      if (oscillator_dataclock_.DidCrossZero()) {
        bit_buffer_.push_back(accumulator_ > 0.f);
        accumulator_ = 0.f;
      }
    }

    oscillator_dataclock_.Step();
    oscillator_subcarrier_.Step();

    sample_num_++;
  }
}

int Subcarrier::NextBit() {
  while (bit_buffer_.size() < 1 && !eof()) DemodulateMoreBits();

  int bit = 0;

  if (bit_buffer_.size() > 0) {
    bit = bit_buffer_.front();
    bit_buffer_.pop_front();
  }

  return bit;
}

bool Subcarrier::eof() const {
  return is_eof_;
}

}  // namespace darc2json
