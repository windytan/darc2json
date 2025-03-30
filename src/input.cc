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
#include "src/input.h"

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace darc2json {

bool MPXReader::eof() const {
  return is_eof_;
}

StdinReader::StdinReader(const Options& options)
    : samplerate_(options.samplerate), feed_thru_(options.feed_thru) {
  is_eof_ = false;
}

std::vector<float> StdinReader::ReadChunk() {
  const int num_read = std::fread(buffer_.data(), sizeof(buffer_[0]), kInputBufferSize, stdin);

  if (feed_thru_)
    std::fwrite(buffer_.data(), sizeof(buffer_[0]), num_read, stdout);

  if (num_read < kInputBufferSize)
    is_eof_ = true;

  std::vector<float> chunk(num_read);
  for (int i = 0; i < num_read; i++) chunk[i] = buffer_[i];

  return chunk;
}

float StdinReader::samplerate() const {
  return samplerate_;
}

SndfileReader::SndfileReader(const Options& options)
    : info_({0, 0, 0, 0, 0, 0}), file_(::sf_open(options.sndfilename.c_str(), SFM_READ, &info_)) {
  is_eof_ = false;
  if (info_.frames == 0) {
    throw std::runtime_error("error: can't open input file");
  } else if (info_.samplerate < 128'000.f) {
    ::sf_close(file_);
    throw std::runtime_error("error: sample rate must be 128000 Hz or higher");
  }
}

SndfileReader::~SndfileReader() {
  ::sf_close(file_);
}

std::vector<float> SndfileReader::ReadChunk() {
  std::vector<float> chunk;
  if (is_eof_)
    return chunk;

  const auto frames_to_read = kInputBufferSize / info_.channels;

  const sf_count_t num_read = sf_readf_float(file_, buffer_.data(), frames_to_read);
  if (num_read != frames_to_read)
    is_eof_ = true;

  if (info_.channels == 1) {
    chunk = std::vector<float>(buffer_.data(), buffer_.data() + num_read);
  } else {
    chunk = std::vector<float>(num_read);
    for (std::size_t i = 0; i < chunk.size(); i++) chunk[i] = buffer_[i * info_.channels];
  }
  return chunk;
}

float SndfileReader::samplerate() const {
  return info_.samplerate;
}

AsciiBitReader::AsciiBitReader(const Options& options)
    : is_eof_(false), feed_thru_(options.feed_thru) {}

int AsciiBitReader::NextBit() {
  int chr = 0;
  while (chr != '0' && chr != '1' && chr != EOF) {
    chr = std::getchar();
    if (feed_thru_)
      std::putchar(chr);
  }

  if (chr == EOF) {
    is_eof_ = true;
    return 0;
  }

  return (chr == '1');
}

bool AsciiBitReader::eof() const {
  return is_eof_;
}

}  // namespace darc2json
