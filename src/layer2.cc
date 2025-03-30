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
#include "src/layer2.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>

#include "src/util.h"

namespace darc2json {

constexpr std::uint16_t kBic1 = 0x135E;
constexpr std::uint16_t kBic2 = 0x74A6;
constexpr std::uint16_t kBic3 = 0xA791;
constexpr std::uint16_t kBic4 = 0xC875;

const Bits kL2CRC = poly_coeffs_to_bits({14, 11, 2, 0});
const Bits kL2HorizontalParity =
    poly_coeffs_to_bits({82, 77, 76, 71, 67, 66, 56, 52, 48, 40, 36, 34, 24, 22, 18, 10, 4, 0});

const std::map<Bits, Bits> parity_syndrome_errors =
    create_bitflip_syndrome_map(272, kL2HorizontalParity);

bool IsValidBic(std::uint16_t word) {
  return (word == kBic1 || word == kBic2 || word == kBic3 || word == kBic4);
}

eBic BicFor(std::uint16_t word) {
  if (word == kBic1)
    return BIC1;
  else if (word == kBic2)
    return BIC2;
  else if (word == kBic3)
    return BIC3;
  else
    return BIC4;
}

Descrambler::Descrambler() {
  constexpr std::array<std::uint16_t, 19> seq_words(
      {0xafaa, 0x814a, 0xf2ee, 0x073a, 0x4f5d, 0x4486, 0x70bd, 0xb343, 0xbc3f, 0xe0f7, 0xc5cc,
       0x8253, 0xb479, 0xf362, 0xa471, 0xb571, 0x3110, 0x0846, 0x1390});

  for (std::size_t n_word = 0; n_word < seq_words.size(); n_word++)
    for (int n_bit = 0; n_bit < 16; n_bit++)
      sequence_[16 * n_word + n_bit] = (seq_words[n_word] >> (15 - n_bit)) & 1;
}

int Descrambler::Descramble(int bit) {
  const int result = bit ^ sequence_[bit_counter_];
  bit_counter_++;
  return result;
}

L2Block::L2Block(eBic _bic) : bic_(_bic), bits_(272) {}

void L2Block::PushBit(int bit) {
  if (bit_counter_ < bits_.size()) {
    bits_[bit_counter_] = descrambler_.Descramble(bit);
    bit_counter_++;
  }
}

bool L2Block::complete() const {
  return bit_counter_ == 272;
}

int L2Block::BicNum() const {
  return bic_ + 1;
}

Bits L2Block::information_bits() const {
  return Bits(bits_.begin(), bits_.begin() + 176);
}

bool L2Block::crc_ok() {
  const Bits syndrome = crc(bits_, kL2HorizontalParity, 176 + 14 + 82);

  bool is_ok = AllBitsZero(syndrome);

  if (!is_ok) {
    if (parity_syndrome_errors.count(syndrome) != 0) {
      const Bits evector = parity_syndrome_errors.at(syndrome);
      for (std::size_t i = 0; i < evector.size(); i++) {
        bits_[i] ^= evector[i];
      }

      is_ok = AllBitsZero(crc(bits_, kL2HorizontalParity, 176 + 14 + 82));
    }
  }

  return is_ok;
}

Layer2::Layer2() : bic_register_(0x0000), block_(BicFor(bic_register_)) {}

std::vector<L2Block> Layer2::PushBit(int bit) {
  std::vector<L2Block> blocks;

  if (in_sync_) {
    block_.PushBit(bit);
    if (block_.complete()) {
      if (block_.crc_ok())
        blocks.push_back(block_);
      in_sync_ = false;
    }
  } else {
    bic_register_ = (bic_register_ << 1) + bit;
    if (IsValidBic(bic_register_)) {
      block_   = L2Block(BicFor(bic_register_));
      in_sync_ = true;
    }
  }

  return blocks;
}

}  // namespace darc2json
