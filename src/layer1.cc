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

#include <cmath>
#include <complex>
#include <cstdio>
#include <deque>
#include <iostream>
#include <tuple>

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif

#include "src/common.h"
#include "src/input.h"
#include "src/liquid_wrappers.h"

namespace darc2json {

namespace {

const float kCarrierFrequency_Hz = 76000.0f;
const float kBitsPerSecond       = 16000.0f;
const int kSamplesPerSymbol      = 8;
const float kAGCBandwidth_Hz     = 500.0f;
const float kAGCInitialGain      = 0.0077f;
const float kLowpassCutoff_Hz    = 11000.0f;
const float kPLLBandwidth_Hz     = 0.01f;
const float kPLLMultiplier       = 12.0f;

float hertz2step(float Hz) {
  return Hz * 2.0f * M_PI / kTargetSampleRate_Hz;
}

#ifdef DEBUG
float step2hertz(float step) {
  return step * kTargetSampleRate_Hz / (2.0f * M_PI);
}
#endif

}  // namespace

Subcarrier::Subcarrier(const Options& options)
    : sample_num_(0),
      resample_ratio_(kTargetSampleRate_Hz / options.samplerate),
      bit_buffer_(),
      fir_lpf_(256, kLowpassCutoff_Hz / kTargetSampleRate_Hz),
      fir_lpf2_(256, 16000 / (kTargetSampleRate_Hz / 2)),
      agc_(kAGCBandwidth_Hz / kTargetSampleRate_Hz, kAGCInitialGain),
      oscillator_subcarrier_(LIQUID_VCO, hertz2step(kCarrierFrequency_Hz)),
      oscillator_dataclock_(LIQUID_VCO, hertz2step(kBitsPerSecond / 2)),
      resampler_(resample_ratio_, 13),
      freqdem_(0.5f),
      is_eof_(false),
      accumulator_(0.f) {
  oscillator_dataclock_.set_pll_bandwidth(kPLLBandwidth_Hz / kTargetSampleRate_Hz);

#ifdef HAVE_SNDFILE
  if (options.input_type == INPUT_MPX_SNDFILE)
    mpx_ = new SndfileReader(options);
  else
#endif
    mpx_ = new StdinReader(options);

  resample_ratio_ = kTargetSampleRate_Hz / mpx_->samplerate();
  resampler_.set_rate(resample_ratio_);
}

Subcarrier::~Subcarrier() {}

void Subcarrier::DemodulateMoreBits() {
  is_eof_ = mpx_->eof();
  if (is_eof_)
    return;

  std::vector<float> inbuffer = mpx_->ReadChunk();

  int num_samples = 0;

  std::vector<std::complex<float>> complex_samples(
      resample_ratio_ <= 1.0f ? inbuffer.size() : inbuffer.size() * resample_ratio_);

  if (resample_ratio_ == 1.0f) {
    for (size_t i = 0; i < inbuffer.size(); i++) complex_samples[i] = inbuffer[i];
    num_samples = inbuffer.size();
  } else {
    int i_resampled = 0;
    for (size_t i = 0; i < inbuffer.size(); i++) {
      std::complex<float> buf[4];
      int num_resampled = resampler_.execute(inbuffer[i], buf);

      for (int j = 0; j < num_resampled; j++) {
        complex_samples[i_resampled] = buf[j];
        i_resampled++;
      }
    }
    num_samples = i_resampled;
  }

  const int decimate_ratio = kTargetSampleRate_Hz / kBitsPerSecond / kSamplesPerSymbol;

  for (int i = 0; i < num_samples; i++) {
    std::complex<float> sample = complex_samples[i];

    std::complex<float> sample_baseband = oscillator_subcarrier_.MixDown(sample);

    fir_lpf_.push(sample_baseband);

    if (sample_num_ % decimate_ratio == 0) {
      std::complex<float> sample_lopass = agc_.execute(fir_lpf_.execute());

      float fmdem = freqdem_.Demodulate(sample_lopass);

      accumulator_ += fmdem * fabs(oscillator_dataclock_.cos());
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

#ifdef DEBUG
float Subcarrier::t() const {
  return sample_num_ / kTargetSampleRate_Hz;
}
#endif

}  // namespace darc2json
