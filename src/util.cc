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
#include "src/util.h"

#include <cassert>
#include <vector>

namespace darc2json {

std::vector<int> bitvector_lsb(std::vector<uint8_t> input) {
  std::vector<int> result;

  for (uint8_t c : input) {
    for (int i = 7; i >= 0; i--) {
      result.push_back((c >> i) & 1);
    }
  }
  return result;
}

std::vector<int> bitvector_msb(std::vector<uint8_t> input) {
  std::vector<int> result;

  for (uint8_t c : input) {
    for (int i = 0; i < 8; i++) {
      result.push_back((c >> i) & 1);
    }
  }
  return result;
}

uint32_t field(const std::vector<int>& bits,
               int start_at, int length) {
  uint32_t result = 0;
  for (int i = 0; i < length; i++) {
    result += (bits[start_at + i] << i);
  }

  return result;
}

void lshift(std::vector<int>& bits) {
  for (size_t i = 0; i < bits.size() - 1; i++)
    bits[i] = bits[i + 1];
  bits[bits.size() - 1] = 0;
}

std::vector<int> crc(std::vector<int> bits, const std::vector<int>& generator) {
  assert(generator.size() > 1);
  std::vector<int> result(generator.size() - 1);

  // Input padding
  for (size_t i = 0; i < generator.size() - 1; i++)
    bits.push_back(0);

  for (size_t i = 0; i < bits.size(); i++) {
    int popped_bit = result[0];
    lshift(result);
    result[result.size() - 1] = bits[i];

    // XOR if shifted-out bit was 1
    if (popped_bit) {
      for (size_t j = 0; j < result.size(); j++)
        result[j] ^= generator[j + 1];
    }
  }

  return result;
}

}  // namespace darc2json
