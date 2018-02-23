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
#include "src/layer3_4.h"

#include <cassert>
#include <iomanip>
#include <iostream>

#include "src/layer2.h"
#include "src/util.h"

namespace darc2json {

const Bits kL3ShortMessageHeaderCRC = poly_coeffs_to_bits({6, 4, 3, 0});
const Bits kL3LongMessageHeaderCRC  = poly_coeffs_to_bits({6, 4, 3, 0});
const Bits kL4LongMessageHeaderCRC  = poly_coeffs_to_bits({6, 4, 3, 0});

std::string TimeString(int hours, int minutes, int seconds) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) <<
        hours << ":" << minutes << ":" << seconds;
  return ss.str();
}

std::string TimePointToString(
    const std::chrono::time_point<std::chrono::system_clock>& timepoint,
    const std::string& format) {
  std::time_t t = std::chrono::system_clock::to_time_t(timepoint);
  std::tm tm = *std::localtime(&t);

  std::string result;
  char buffer[64];
  if (strftime(buffer, sizeof(buffer), format.c_str(), &tm) > 0) {
    result = std::string(buffer);
  } else {
    result = "(format error)";
  }

  return result;
}

SechBlock::SechBlock(const Bits& info_bits) :
  is_last_fragment_(field(info_bits, 5, 1)),
  data_update_(field(info_bits, 6, 2)),
  country_id_(field(info_bits, 8, 4)),
  data_type_(field(info_bits, 12, 4)),
  network_id_(field(info_bits, 16, 4)),
  block_num_(field(info_bits, 20, 4)),
  data_(info_bits.begin() + 24, info_bits.begin() + 24 + 19*8) {
}

bool SechBlock::is_last_fragment() const {
  return is_last_fragment_;
}

int SechBlock::block_num() const {
  return block_num_;
}

bool SechBlock::follows_in_sequence(const SechBlock& previous) const {
  return data_update_ == previous.data_update_ &&
         country_id_ == previous.country_id_ &&
         data_type_ == previous.data_type_ &&
         network_id_ == previous.network_id_ &&
         block_num_ == previous.block_num_ + 1;
}

int SechBlock::country_id() const {
  return country_id_;
}

int SechBlock::data_type() const {
  return data_type_;
}

int SechBlock::network_id() const {
  return network_id_;
}

Bits SechBlock::data_bits() const {
  return data_;
}

ServiceMessage::ServiceMessage() {
}

void ServiceMessage::push_block(const SechBlock& block) {
  if (block.block_num() == 0) {
    blocks_.clear();
    is_complete_ = false;
  }

  if (block.block_num() == static_cast<int>(blocks_.size()) &&
      (block.block_num() == 0 || block.follows_in_sequence(blocks_.back()))) {
    blocks_.push_back(block);

    if (block.is_last_fragment())
      is_complete_ = true;
  } else {
    blocks_.clear();
    is_complete_ = false;
  }
}

bool ServiceMessage::is_complete() const {
  return is_complete_;
}

Bits ServiceMessage::data_bits() const {
  Bits result;
  for (SechBlock block : blocks_) {
    for (int b : block.data_bits())
      result.push_back(b);
  }

  return result;
}

int ServiceMessage::country_id() const {
  return blocks_.empty() ? 0 : blocks_[0].country_id();
}

int ServiceMessage::network_id() const {
  return blocks_.empty() ? 0 : blocks_[0].network_id();
}

int ServiceMessage::data_type() const {
  return blocks_.empty() ? 0 : blocks_[0].data_type();
}

Json::Value ServiceMessage::to_json() const {
  Json::Value json;

  if (!is_complete())
    return json;

  std::vector<std::string> sech_types(
      {"COT", "AFT", "SAFT", "TDPNT", "SNT", "TDT", "SCOT"});

  Bits data = data_bits();

  json["cid"] = country_id();
  json["nid"] = network_id();

  json["service_message"]["type"] =
    (data_type() < static_cast<int>(sech_types.size()) ?
                   sech_types.at(data_type()) : "err");

  int ecc = field(data, 0, 8);
  int tseid = field(data, 8, 7);
  //int message_len = field(data, 15, 9);
  json["service_message"]["ecc"] = ecc;
  json["service_message"]["tse_id"] = tseid;

  if (data_type() == kTypeTDT) {
    Bits time_bits(data.begin() + 3*8, data.begin() + 7*8 + 1);
    Bits date_bits(data.begin() + 7*8, data.begin() + 10*8 + 1);

    /*bool eta = field(time_bits, 0, 1);
    int hours = field(time_bits, 1, 5);
    int minutes = field(time_bits, 6, 6);
    int seconds = field(time_bits, 12, 6);
    int lto = field(time_bits, 18, 6);*/

    int name_len = field(date_bits, 2*8 + 2, 4);
    std::string name;
    if (static_cast<int>(data.size()) >= (10 + name_len) * 8) {
      for (int i = 0; i < name_len; i++) {
        char c = field(data, (10 + i) * 8, 8);
        name += c;
      }
    }
    json["service_message"]["network_name"] = name;
    //json["service_message"]["time"] = TimeString(hours, minutes, seconds);

    bool has_position = field(date_bits, 2*9 + 6, 1);

    if (has_position) {

    }
  }

  return json;
}

ShortBlock::ShortBlock(const Bits& info_bits) :
    is_last_fragment_(field(info_bits, 5, 1)),
    sequence_counter_(field(info_bits, 6, 4)) {
  Bits crc_rx(info_bits.begin() + 10, info_bits.begin() + 10 + 6);
  Bits header_bits(info_bits.begin(), info_bits.begin() + 10);
  Bits crc_calc = crc(header_bits, kL3ShortMessageHeaderCRC);

  header_crc_ok_ = BitsEqual(crc_rx, crc_calc);

  /*printf("crc_rx(%s)", BitString(crc_rx).c_str());
  printf("crc_calc(%s)", BitString(crc_calc).c_str());

  printf(" LF[%s] SC:%d CRC_OK[%s]\n",
      is_last_fragment_ ? "x" : " ",
     sequence_counter_,
      header_crc_ok_ ? "x" : " ");*/
}

bool ShortBlock::is_last_fragment() const {
  return is_last_fragment_;
}

bool ShortBlock::header_crc_ok() const {
  return header_crc_ok_;
}

bool ShortBlock::follows_in_sequence(const ShortBlock& previous) const {
  return sequence_counter_ == previous.sequence_counter_ + 1;
}

ShortMessage::ShortMessage() {
}

void ShortMessage::push_block(const ShortBlock& block) {
  blocks_.push_back(block);
}

bool ShortMessage::is_complete() const {
  bool is_complete = false;

  if (blocks_.empty()) {
    is_complete = false;
  } else if (blocks_.size() == 1) {
    is_complete = blocks_[0].is_last_fragment();
  } else {
    is_complete = blocks_[0].is_last_fragment();
    for (size_t i = 1; i < blocks_.size(); i++) {
      if (!blocks_[i].follows_in_sequence(blocks_[i - 1]))
        is_complete = false;
    }
  }

  return is_complete;
}

LongBlock::LongBlock(const Bits& info_bits) :
    is_last_fragment_(field(info_bits, 5, 1)),
    sequence_counter_(field(info_bits, 6, 4)),
    data_(info_bits.begin() + 16, info_bits.end()) {
  Bits crc_rx(info_bits.begin() + 10, info_bits.begin() + 10 + 6);
  Bits header_bits(info_bits.begin(), info_bits.begin() + 10);
  Bits crc_calc = crc(header_bits, kL3LongMessageHeaderCRC);

  bool di = field(info_bits, 4, 1);
  l3_header_crc_ok_ = BitsEqual(crc_rx, crc_calc);

  /*printf(" LF[%s] DI[%s] SC:%02d L3_CRC_OK[%s] ",
      is_last_fragment_ ? "x" : " ",
      di ? "x" : " ",
      sequence_counter_,
      header_crc_ok_ ? "x" : " ");
  printf("%s\n", BitString(data_).c_str());*/
}

bool LongBlock::is_last_fragment() const {
  return is_last_fragment_;
}

bool LongBlock::header_crc_ok() const {
  return l3_header_crc_ok_;
}

bool LongBlock::follows_in_sequence(const LongBlock& previous) const {
  return sequence_counter_ == ((previous.sequence_counter_ + 1) % 16) &&
         header_crc_ok() && previous.header_crc_ok() &&
         !previous.is_last_fragment();
}

Bits LongBlock::data_bits() const {
  return data_;
}

LongMessage::LongMessage() : is_complete_(false) {
}

void LongMessage::push_block(const LongBlock& block) {
  if (blocks_.size() > 0 && !block.follows_in_sequence(blocks_.back())) {
    blocks_.clear();
    bytes_.clear();
    header_crc_ok_ = false;
    is_complete_ = false;
  }

  blocks_.push_back(block);
  for (size_t n_byte = 0; n_byte < block.data_bits().size() / 8; n_byte++) {
    bytes_.push_back(field(block.data_bits(), n_byte * 8, 8));
  }

  if (block.is_last_fragment()) {
    parse_header();
  }
}

void LongMessage::parse_header() {
  header_crc_ok_ = false;

  if (blocks_.empty())
    return;

  int  ri   = bytes_[0] >> 6;
  int  ci   = (bytes_[0] >> 4) & 0x3;
  bool ext  = (bytes_[0] >> 1) & 1;
  int  add  = ((bytes_[0] & 1) << 8) + bytes_[1];
  bool com  = (bytes_[2 + ext] >> 7) & 1;
  bool caf  = (bytes_[2 + ext] >> 6) & 1;
  int  dlen = ((bytes_[2 + ext] & 0x3f) << 2) + (bytes_[3 + ext] >> 6);

  uint8_t crc_rx = bytes_[3 + ext] & 0x3f;
  Bits header_bits;
  for (int n_byte = 0; n_byte < 4 + ext; n_byte++) {
    for (int n_bit = 0; n_bit < 8; n_bit++) {
      header_bits.push_back((bytes_[n_byte] >> (7-n_bit)) & 1);
    }
  }
  header_bits.resize((3 + ext) * 8 + 2);
  uint8_t crc_calc =
      field_rev(crc(header_bits, kL4LongMessageHeaderCRC), 0, 6);

  bool crc_ok = (crc_rx == crc_calc);
  bool complete = (static_cast<int>(bytes_.size()) >= dlen);

  /*printf("L4: RI:%d CI:%d ext:[%s] add:%03x com:[%s] caf:[%s] dlen:%3d "
         "(rx: %3ld) L4_CRC_OK[%s]",
      ri,
      ci,
      ext ? "x" : " ",
      add,
      com ? "x" : " ",
      caf ? "x" : " ",
      dlen,
      bytes_.size(),
      crc_ok ? "x" : " ");
  if (complete)
    printf(" COMPLETE");

  printf("\n");*/
  if (crc_ok && complete) {
    is_complete_ = true;
    bytes_.resize(dlen);
    /*printf("data: ");
    for (uint8_t c : bytes_)
      printf("%02x ", c);
    for (uint8_t c : bytes_)
      printf("%c", c);*/

  }
  //printf("\n\n");
}

Bits LongMessage::bits() const {
  Bits result;
  for (LongBlock block : blocks_)
    for (int bit : block.data_bits())
      result.push_back(bit);

  return result;
}

bool LongMessage::is_complete() const {
  return is_complete_;
}

Json::Value LongMessage::to_json() const {
  Json::Value json;

  std::stringstream data_hex;
  for (int c : bytes_)
    data_hex << std::setfill('0') << std::setw(2) << std::hex <<
             c << " ";
  json["long_message"]["l4data"] = data_hex.str();

  return json;
}

Layer3::Layer3(const Options& options) :
    options_(options), writer_builder_() {
  writer_builder_["indentation"] = "";
  writer_ =
      std::unique_ptr<Json::StreamWriter>(writer_builder_.newStreamWriter());
}

Layer3::~Layer3() {
}

void Layer3::push_block(const L2Block& l2block) {
  Bits info_bits = l2block.information_bits();

  uint16_t silch = field(info_bits, 0, 4);

  if (silch == 0x8) {
    service_message_.push_block(SechBlock(info_bits));

    if (service_message_.is_complete())
      print_line(service_message_.to_json());

  } else if (silch == 0x9) {
    //printf("SMCh ");
    short_message_.push_block(ShortBlock(info_bits));
  } else if (silch == 0xA) {
    //printf("LMCh ");
    long_message_.push_block(LongBlock(info_bits));

    if (long_message_.is_complete())
      print_line(long_message_.to_json());

  } else if (silch == 0xB) {
    //bool is_realtime = field(info_bits, 4, 1);
    int subchannel = field(info_bits, 5, 3);
    if (subchannel == 0x0) {
      Bits data;
      data = Bits(info_bits.begin() + 8, info_bits.end());
      Json::Value json;

      std::stringstream ss;
      for (size_t nbit = 0; nbit < data.size(); nbit += 4)
        ss << std::setfill('0') << std::setw(1) << std::hex <<
              field(data, nbit, 4);

      json["block_app"]["data"] = ss.str();
      print_line(json);
    }
    //printf("subch:%d ", subchannel);
  } else {

  }
}

void Layer3::print_line(Json::Value json) {
  if (options_.timestamp)
    json["rx_time"] = TimePointToString(std::chrono::system_clock::now(),
                                        options_.time_format);
  std::stringstream ss;
  writer_->write(json, &ss);
  ss << '\n';
  std::cout << ss.str() << std::flush;
}

}  // namespace darc2json
