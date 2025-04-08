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
#ifndef LAYER1_H_
#define LAYER1_H_

#include <complex>
#include <deque>
#include <utility>
#include <vector>

#include "config.h"

#include "src/common.h"
#include "src/input.h"
#include "src/liquid_wrappers.h"

namespace darc2json {

class Subcarrier {
 public:
  explicit Subcarrier(const Options& options);
  ~Subcarrier();
  int NextBit();
  bool eof() const;
#ifdef DEBUG
  float t() const;
#endif

 private:
  void DemodulateMoreBits();
  int sample_num_;
  float resample_ratio_;

  std::deque<int> bit_buffer_;

  liquid::FIRFilter fir_lpf_;
  liquid::FIRFilter fir_lpf2_;
  liquid::AGC agc_;
  liquid::NCO oscillator_subcarrier_;
  liquid::NCO oscillator_dataclock_;
  liquid::Resampler resampler_;
  liquid::Freqdem freqdem_;

  bool is_eof_;
  float accumulator_;

  std::complex<float> prev_sym_;

  MPXReader* mpx_;
};

}  // namespace darc2json

#endif  // LAYER1_H_
