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
#ifndef LAYER2_H_
#define LAYER2_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "config.h"

#include "src/common.h"
#include "src/util.h"

namespace darc2json {

enum eBic { BIC1, BIC2, BIC3, BIC4 };

std::uint32_t field(const Bits& bits, int start_at, int length);

class Descrambler {
 public:
  Descrambler();
  int Descramble(int bit);

 private:
  std::array<int, 304> sequence_{};
  int bit_counter_{};
};

class L2Block {
 public:
  L2Block(eBic _bic);
  ~L2Block() = default;
  void PushBit(int bit);
  bool complete() const;
  int BicNum() const;
  bool crc_ok();
  Bits information_bits() const;

 private:
  eBic bic_;
  Bits bits_;
  std::size_t bit_counter_{};
  Descrambler descrambler_{};
};

class Layer2 {
 public:
  Layer2();
  ~Layer2() = default;
  std::vector<L2Block> PushBit(int bit);

 private:
  std::uint16_t bic_register_;
  L2Block block_;
  bool in_sync_{};
};

}  // namespace darc2json

#endif  // LAYER2_H_
