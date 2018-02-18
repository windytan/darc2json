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

namespace darc2json {

class SechBlock {
 public:
  SechBlock(const std::vector<int>& info_bits);
  bool is_last_fragment() const;
  int block_num() const;
  int country_id() const;
  int data_type() const;
  int network_id() const;
  std::vector<int> data_bits() const;

  bool follows_in_sequence(const SechBlock& previous) const;

 private:
  bool is_last_fragment_;
  int data_update_;
  int country_id_;
  int data_type_;
  int network_id_;
  int block_num_;
  std::vector<int> data_;
};

class ServiceMessage {
 public:
  ServiceMessage();
  void push_block(const SechBlock& block);
  bool is_complete() const;
  void print(std::unique_ptr<Json::StreamWriter>& writer) const;
  std::vector<int> data_bits() const;

 private:
  std::vector<SechBlock> blocks_;
};

class Layer3 {
 public:
  Layer3();
  ~Layer3();
  void push_block(const L2Block& block);

 private:
  ServiceMessage service_message_;

  Json::StreamWriterBuilder writer_builder_;
  std::unique_ptr<Json::StreamWriter> writer_;
  Json::Value json_;
};

}  // namespace darc2json

#endif  // LAYER3_H_
