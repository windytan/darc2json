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
#include "src/liquid_wrappers.h"

#include "config.h"

#include <cassert>
#include <complex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
// https://github.com/jgaeddert/liquid-dsp/issues/229
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern "C" {
#include "liquid/liquid.h"
}
#pragma clang diagnostic pop

namespace liquid {

AGC::AGC(float bw, float initial_gain) {
  object_ = agc_crcf_create();
  agc_crcf_set_bandwidth(object_, bw);
  agc_crcf_set_gain(object_, initial_gain);
}

AGC::~AGC() {
  agc_crcf_destroy(object_);
}

std::complex<float> AGC::execute(std::complex<float> s) {
  std::complex<float> result;
  agc_crcf_execute(object_, s, &result);
  return result;
}

float AGC::gain() {
  return agc_crcf_get_gain(object_);
}

FIRFilter::FIRFilter(int len, float fc, float As, float mu) {
  assert(fc >= 0.0f && fc <= 0.5f);
  assert(As > 0.0f);
  assert(mu >= -0.5f && mu <= 0.5f);

  object_ = firfilt_crcf_create_kaiser(len, fc, As, mu);
  firfilt_crcf_set_scale(object_, 2.0f * fc);
}

FIRFilter::~FIRFilter() {
  firfilt_crcf_destroy(object_);
}

void FIRFilter::push(std::complex<float> s) {
  firfilt_crcf_push(object_, s);
}

std::complex<float> FIRFilter::execute() {
  std::complex<float> result;
  firfilt_crcf_execute(object_, &result);
  return result;
}

NCO::NCO(liquid_ncotype type, float freq) : object_(nco_crcf_create(type)), did_cross_zero_(false) {
  nco_crcf_set_frequency(object_, freq);
}

NCO::~NCO() {
  nco_crcf_destroy(object_);
}

std::complex<float> NCO::MixDown(std::complex<float> s) {
  std::complex<float> result;
  nco_crcf_mix_down(object_, s, &result);
  return result;
}

std::complex<float> NCO::MixUp(std::complex<float> s) {
  std::complex<float> result;
  nco_crcf_mix_up(object_, s, &result);
  return result;
}

void NCO::MixBlockDown(std::complex<float>* x, std::complex<float>* y, int n) {
  nco_crcf_mix_block_down(object_, x, y, n);
}

void NCO::Step() {
  const float prev_value = nco_crcf_cos(object_);
  nco_crcf_step(object_);
  const float current_value = nco_crcf_cos(object_);

  if ((prev_value <= 0.f && current_value > 0.f) || (prev_value > 0.f && current_value <= 0.f))
    did_cross_zero_ = true;
}

void NCO::setPLLBandwidth(float bw) {
  nco_crcf_pll_set_bandwidth(object_, bw);
}

void NCO::StepPLL(float dphi) {
  nco_crcf_pll_step(object_, dphi);
}

float NCO::frequency() {
  return nco_crcf_get_frequency(object_);
}

bool NCO::DidCrossZero() {
  const bool temp = did_cross_zero_;
  did_cross_zero_ = false;
  return temp;
}

float NCO::cos() {
  return nco_crcf_cos(object_);
}

SymSync::SymSync(liquid_firfilt_type ftype, unsigned k, unsigned m, float beta,
                 unsigned num_filters)
    : object_(symsync_crcf_create_rnyquist(ftype, k, m, beta, num_filters)) {}

SymSync::~SymSync() {
  symsync_crcf_destroy(object_);
}

void SymSync::set_bandwidth(float bw) {
  symsync_crcf_set_lf_bw(object_, bw);
}

void SymSync::set_output_rate(unsigned r) {
  symsync_crcf_set_output_rate(object_, r);
}

std::vector<std::complex<float>> SymSync::execute(std::complex<float>* in) {
  std::complex<float> s_out[8];
  unsigned n_out = 0;
  symsync_crcf_execute(object_, in, 1, &s_out[0], &n_out);

  std::vector<std::complex<float>> result(std::begin(s_out), std::end(s_out));
  result.resize(n_out);
  return result;
}

FSKdem::FSKdem(unsigned k, float bw) : object_(fskdem_create(1, k, bw)) {}

FSKdem::~FSKdem() {
  fskdem_destroy(object_);
}

Freqdem::Freqdem(float factor) : object_(freqdem_create(factor)) {}

Freqdem::~Freqdem() {
  freqdem_destroy(object_);
}

float Freqdem::Demodulate(std::complex<float> in) {
  float out;
  freqdem_demodulate(object_, in, &out);
  return out;
}

unsigned int FSKdem::Demodulate(std::complex<float> in) {
  return fskdem_demodulate(object_, &in);
}

Modem::Modem(modulation_scheme scheme) : object_(modem_create(scheme)) {}

Modem::~Modem() {
  modem_destroy(object_);
}

unsigned int Modem::Demodulate(std::complex<float> sample) {
  unsigned symbol_out;

  modem_demodulate(object_, sample, &symbol_out);

  return symbol_out;
}

float Modem::phase_error() {
  return modem_get_demodulator_phase_error(object_);
}

Resampler::Resampler(float ratio, int length)
    : object_(resamp_crcf_create(ratio, length, 0.47f, 60.0f, 32)) {
  assert(ratio <= 2.0f);
}

Resampler::~Resampler() {
  resamp_crcf_destroy(object_);
}

void Resampler::set_rate(float rate) {
  resamp_crcf_set_rate(object_, rate);
}

unsigned int Resampler::execute(std::complex<float> in, std::complex<float>* out) {
  unsigned int num_written;
  resamp_crcf_execute(object_, in, out, &num_written);

  return num_written;
}

}  // namespace liquid
