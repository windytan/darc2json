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

namespace darcdec {

const uint16_t kBic1 = 0x135E;
const uint16_t kBic2 = 0x74A6;
const uint16_t kBic3 = 0xA791;
const uint16_t kBic4 = 0xC875;

bool IsValidBic(uint16_t word) {
  return (word == kBic1 || word == kBic2 || word == kBic3 || word == kBic4);
}

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

bool L2Block::crc_ok() const {
  std::vector<int> information_bits(bits_.begin(), bits_.begin() + 176);

  std::vector<int> crc_calc =
    crc(information_bits, {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1});
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

uint16_t L2Block::silch() const {
  return field(bits_, 0, 4);
}

void L2Block::print() const {
  if (bic_ == BIC4) {
    printf("(vertical parity)");
  } else if (crc_ok()) {

    bool last_fragment;
    int data_update, country_id, data_type, network_id, block_num;
    std::vector<std::string> sech_types(
        {"COT", "AFT", "SAFT", "TDPNT", "SNT", "TDT", "SCOT"});

    if (silch() == 0x8) {
      printf("SeCH ");
      last_fragment = field(bits_, 5, 1);
      data_update   = field(bits_, 6, 2);
      country_id    = field(bits_, 8, 4);
      data_type     = field(bits_, 12, 4);
      network_id    = field(bits_, 16, 4);
      block_num     = field(bits_, 20, 4);
      printf("LF[%s] DUP:%d CID:%d NID:%d BLN:%d ",
          last_fragment ? "x" : " ", data_update, country_id, network_id, block_num);
      printf("TYPE: %s", data_type < (int)sech_types.size() ? sech_types[data_type].c_str() : "err");

      if (block_num == 0 && last_fragment) {
        std::vector<int> data_bits(bits_.begin() + 24, bits_.end());
        int ecc = field(data_bits, 0, 8);
        int tseid = field(data_bits, 8, 7);
        int message_len = field(data_bits, 15, 9);
        printf(" len:%d ", message_len);


        if (data_type == 0x5) {
          std::vector<int> time_bits(data_bits.begin() + 3*8, data_bits.begin() + 7*8);
          std::vector<int> date_bits(data_bits.begin() + 7*8, data_bits.begin() + 10*8);

          int name_len = field(date_bits, 2*8 + 2, 4);
          printf ("name:\"");
          for (int i = 0; i < name_len; i++) {
            char c = field(data_bits, (10 + i) * 8, 8);
            printf("%c", c);
          }
          printf("\" ");

          bool has_position = field(date_bits, 2*9 + 6, 1);
          printf("has_position:%d ",has_position);

          if (has_position) {

          }
        }
      }
    } else if (silch() == 0x9) {
        printf("SMCh ");
    } else if (silch() == 0xA) {
        printf("LMCh ");
    } else if (silch() == 0xB) {
        printf("BMCh ");
        bool is_realtime = field(bits_, 4, 1);
        int subchannel = field(bits_, 5, 3);
        printf("subch:%d ", subchannel);
    } else {
      for (int i = 0; i < 4; i++)
        printf("%d", bits_[i]);
      printf(" ");
    }

    printf(" crc %s", crc_ok() ? "OK" : "FAIL");

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

}  // namespace darcdec
