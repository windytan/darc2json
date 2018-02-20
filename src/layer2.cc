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

#include <cassert>

namespace darc2json {

const uint16_t kBic1 = 0x135E;
const uint16_t kBic2 = 0x74A6;
const uint16_t kBic3 = 0xA791;
const uint16_t kBic4 = 0xC875;

bool IsValidBic(uint16_t word) {
  return (word == kBic1 || word == kBic2 || word == kBic3 || word == kBic4);
}


eBic BicFor(uint16_t word) {
  if (word == kBic1)
    return BIC1;
  else if (word == kBic2)
    return BIC2;
  else if (word == kBic3)
    return BIC3;
  else
    return BIC4;
}

Descrambler::Descrambler() : bit_counter_(0) {
  int wordnum = 0;
  for (uint16_t word : { 0xafaa, 0x814a, 0xf2ee, 0x073a, 0x4f5d, 0x4486, 0x70bd,
                         0xb343, 0xbc3f, 0xe0f7, 0xc5cc, 0x8253, 0xb479, 0xf362,
                         0xa471, 0xb571, 0x3110, 0x0846, 0x1390 }) {
    for (int i = 0; i < 16; i++) {
      sequence_[16*wordnum + i] = (word >> (15 - i)) & 1;
    }
    wordnum++;
  }
}

int Descrambler::Descramble(int bit) {
  int result = bit ^ sequence_[bit_counter_];
  bit_counter_++;
  return result;
}

L2Block::L2Block(eBic _bic) : bic_(_bic), bits_(272), bit_counter_(0),
                          descrambler_() {
}

L2Block::~L2Block() {
}

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

std::vector<int> L2Block::information_bits() const {
  return std::vector<int>(bits_.begin(), bits_.begin() + 176);
}

bool L2Block::crc_ok() const {
  std::vector<int> crc_calc =
    crc(information_bits(), {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1});
  std::vector<int> crc_rx(bits_.begin() + 176, bits_.begin() + 176 + 14);

  bool match = true;
  for (size_t i = 0; i < crc_calc.size(); i++) {
    if (crc_calc[i] != crc_rx[i]) {
      match = false;
      break;
    }
  }

  return match;
}

void L2Block::print() const {
  if (bic_ == BIC4) {
    printf("(vertical parity)");
  } else if (crc_ok()) {

    /*for (int i = 4; i < 176; i++)
      printf("%d", bits_[i]);
    printf("   ");

    for (int i = 176; i < 176+14; i++)
      printf("%d", bits_[i]);
    printf(" ");

    for (int i = 176+14; i < 176+14+82; i++)
      printf("%d", bits_[i]);*/
  }
  printf("\n");
}

Layer2::Layer2() : bic_register_(0x0000), block_(BicFor(bic_register_)),
                   in_sync_(false) {
}

Layer2::~Layer2() {
}

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
      block_ = L2Block(BicFor(bic_register_));
      in_sync_ = true;
    }
  }

  return blocks;
}

}  // namespace darc2json
