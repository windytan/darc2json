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
#ifndef LAYER3_H_
#define LAYER3_H_

#include <array>
#include <vector>

#include <json/json.h>

#include "config.h"

#include "src/common.h"
#include "src/layer2.h"
#include "src/util.h"

namespace darc2json {

enum eSechDataType {
  kTypeCOT = 0, kTypeAFT, kTypeSAFT, kTypeTDPNT, kTypeSNT, kTypeTDT, kTypeSCOT
};

class SechBlock {
 public:
  SechBlock(const Bits& info_bits);
  bool is_last_fragment() const;
  int block_num() const;
  int country_id() const;
  int data_type() const;
  int network_id() const;
  Bits data_bits() const;

  bool follows_in_sequence(const SechBlock& previous) const;

 private:
  bool is_last_fragment_;
  int data_update_;
  int country_id_;
  int data_type_;
  int network_id_;
  int block_num_;
  Bits data_;
};

class ServiceMessage {
 public:
  ServiceMessage();
  void push_block(const SechBlock& block);
  bool is_complete() const;
  Json::Value to_json() const;
  Bits data_bits() const;
  int country_id() const;
  int network_id() const;
  int data_type() const;

 private:
  std::vector<SechBlock> blocks_;
  bool is_complete_;
};

class ShortBlock {
 public:
  ShortBlock(const Bits& info_bits);
  bool is_last_fragment() const;
  bool header_crc_ok() const;

  bool follows_in_sequence(const ShortBlock& previous) const;

 private:
  bool is_last_fragment_;
  int sequence_counter_;
  bool header_crc_ok_;
  Bits data_;
};

class ShortMessage {
 public:
  ShortMessage();
  void push_block(const ShortBlock& block);
  bool is_complete() const;

 private:
  std::vector<ShortBlock> blocks_;
};

class LongBlock {
 public:
  LongBlock(const Bits& info_bits);
  bool is_last_fragment() const;
  bool header_crc_ok() const;
  Bits data_bits() const;

  bool follows_in_sequence(const LongBlock& previous) const;

 private:
  bool is_last_fragment_;
  int sequence_counter_;
  bool l3_header_crc_ok_;
  Bits data_;
};

class LongMessage {
 public:
  LongMessage();
  void push_block(const LongBlock& block);
  bool is_complete() const;
  Json::Value to_json() const;
  Bits bits() const;
  Bytes bytes() const;

 private:
  void parse_header();

  bool is_complete_;
  std::vector<LongBlock> blocks_;
  Bytes bytes_;
  bool header_crc_ok_;
};

class Layer3 {
 public:
  Layer3(const Options& options);
  ~Layer3();
  void push_block(const L2Block& block);
  void print_line(Json::Value json);

 private:
  Options options_;
  ServiceMessage service_message_;
  ShortMessage short_message_;
  LongMessage long_message_;

  Json::StreamWriterBuilder writer_builder_;
  std::unique_ptr<Json::StreamWriter> writer_;
};

}  // namespace darc2json

#endif  // LAYER3_H_
