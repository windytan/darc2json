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
#ifndef COMMON_H_
#define COMMON_H_

#include <string>

namespace darc2json {

constexpr float kTargetSampleRate_Hz = 228'000.0f;
constexpr int kNumBlerAverageGroups  = 12;

enum class InputType { MpxStdin, MpxSndfile, AsciiBits, Hex };

enum class OutputType { Hex, Json };

struct Options {
  bool feed_thru{};
  bool show_partial{};
  bool just_exit{};
  bool timestamp{};
  bool bler{};
  float samplerate{kTargetSampleRate_Hz};
  InputType input_type{InputType::MpxStdin};
  OutputType output_type{OutputType::Json};
  std::string sndfilename;
  std::string time_format;
};

}  // namespace darc2json
#endif  // COMMON_H_
