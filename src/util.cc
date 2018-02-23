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
#include <map>
#include <vector>

namespace darc2json {

// Convert generator polynomial (x^6 + x^4 + x^3 + 1) coefficients
// ({6, 4, 3, 0}) to bitstring ({1, 0, 1, 1, 0, 0, 1})
Bits poly_coeffs_to_bits(const std::vector<int>& coeffs) {
  Bits bits(coeffs.at(0) + 1);
  for (int c : coeffs)
    bits.at(bits.size() - c - 1) = 1;

  return bits;
}

Bits bitvector_lsb(std::vector<uint8_t> input) {
  Bits result;

  for (uint8_t c : input) {
    for (int i = 7; i >= 0; i--) {
      result.push_back((c >> i) & 1);
    }
  }
  return result;
}

Bits bitvector_msb(std::vector<uint8_t> input) {
  Bits result;

  for (uint8_t c : input) {
    for (int i = 0; i < 8; i++) {
      result.push_back((c >> i) & 1);
    }
  }
  return result;
}

uint32_t field(const Bits& bits,
               int start_at, int length) {
  assert (length <= 32);
  uint32_t result = 0;
  for (int i = 0; i < length; i++) {
    result += (bits.at(start_at + i) << i);
  }

  return result;
}

uint32_t field_rev(const Bits& bits,
                   int start_at, int length) {
  assert (length <= 32);
  uint32_t result = 0;
  for (int i = 0; i < length; i++) {
    result += (bits.at(start_at + i) << (length - 1 - i));
  }

  return result;
}

void lshift(Bits& bits) {
  for (size_t i = 0; i < bits.size() - 1; i++)
    bits[i] = bits[i + 1];
  bits[bits.size() - 1] = 0;
}

Bits _crc(const Bits& bits, const Bits& generator) {
  Bits result(generator.size() - 1);

  for (int bit : bits) {
    int popped_bit = result[0];
    lshift(result);
    result[result.size() - 1] = bit;

    // XOR if shifted-out bit was 1
    if (popped_bit) {
      for (size_t j = 0; j < result.size(); j++)
        result[j] ^= generator[j + 1];
    }
  }

  return result;
}

// Normal CRC calculation is done on zero-padded data
Bits crc(Bits bits, const Bits& generator) {
  for (size_t i = 0; i < generator.size() - 1; i++)
    bits.push_back(0);

  return _crc(bits, generator);
}

// Syndrome calculation is done on data + received CRC + parity
Bits syndrome(const Bits& bits, const Bits& generator) {
  return _crc(bits, generator);
}

bool BitsEqual(const Bits& bits1, const Bits& bits2) {
  bool match = true;

  if (bits1.size() == bits2.size()) {
    for (size_t i = 0; i < bits1.size(); i++) {
      if (bits1[i] != bits2[i]) {
        match = false;
        break;
      }
    }
  } else {
    match = false;
  }

  return match;
}

std::string BitString(const Bits& bits) {
  std::string result;
  for (int b : bits)
    result += std::to_string(b);
  return result;
}

const std::map<Bits, Bits> create_bitflip_syndrome_map(size_t len,
                                                       const Bits& generator) {
  std::map<Bits, Bits> result;

  for (size_t i = 0; i < len; i++) {
    Bits evector(len);
    evector.at(i) = 1;

    result[syndrome(evector, generator)] = evector;
  }

  return result;
}

}  // namespace darc2json
