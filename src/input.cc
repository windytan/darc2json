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

#include <iostream>
#include <stdexcept>
#include <string>

namespace darc2json {

namespace {

const int kInputBufferSize = 4096;

}

bool MPXReader::eof() const {
  return is_eof_;
}

StdinReader::StdinReader(const Options& options)
    : samplerate_(options.samplerate),
      buffer_(new(std::nothrow) int16_t[kInputBufferSize]),
      feed_thru_(options.feed_thru) {
  is_eof_ = false;
}

StdinReader::~StdinReader() {
  delete[] buffer_;
}

std::vector<float> StdinReader::ReadChunk() {
  int num_read = fread(buffer_, sizeof(buffer_[0]), kInputBufferSize, stdin);

  if (feed_thru_)
    fwrite(buffer_, sizeof(buffer_[0]), num_read, stdout);

  if (num_read < kInputBufferSize)
    is_eof_ = true;

  std::vector<float> chunk(num_read);
  for (int i = 0; i < num_read; i++) chunk[i] = buffer_[i];

  return chunk;
}

float StdinReader::samplerate() const {
  return samplerate_;
}

#ifdef HAVE_SNDFILE
SndfileReader::SndfileReader(const Options& options)
    : info_({0, 0, 0, 0, 0, 0}),
      file_(sf_open(options.sndfilename.c_str(), SFM_READ, &info_)),
      buffer_(new(std::nothrow) float[info_.channels * kInputBufferSize]) {
  is_eof_ = false;
  if (info_.frames == 0) {
    std::cerr << "error: couldn't open " << options.sndfilename << '\n';
    is_eof_ = true;
  }
  if (info_.samplerate < 128000.f) {
    std::cerr << "error: sample rate must be 128000 Hz or higher" << '\n';
    is_eof_ = true;
  }
}

SndfileReader::~SndfileReader() {
  sf_close(file_);
  delete[] buffer_;
}

std::vector<float> SndfileReader::ReadChunk() {
  std::vector<float> chunk;
  if (is_eof_)
    return chunk;

  sf_count_t num_read = sf_readf_float(file_, buffer_, kInputBufferSize);
  if (num_read != kInputBufferSize)
    is_eof_ = true;

  if (info_.channels == 1) {
    chunk = std::vector<float>(buffer_, buffer_ + num_read);
  } else {
    chunk = std::vector<float>(num_read);
    for (size_t i = 0; i < chunk.size(); i++) chunk[i] = buffer_[i * info_.channels];
  }
  return chunk;
}

float SndfileReader::samplerate() const {
  return info_.samplerate;
}
#endif

AsciiBitReader::AsciiBitReader(const Options& options)
    : is_eof_(false), feed_thru_(options.feed_thru) {}

AsciiBitReader::~AsciiBitReader() {}

int AsciiBitReader::NextBit() {
  int chr = 0;
  while (chr != '0' && chr != '1' && chr != EOF) {
    chr = getchar();
    if (feed_thru_)
      putchar(chr);
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
