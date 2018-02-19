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
#include "src/layer3.h"

#include <cassert>
#include <iomanip>
#include <iostream>

#include "src/layer2.h"

namespace darc2json {

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

SechBlock::SechBlock(const std::vector<int>& info_bits) :
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

std::vector<int> SechBlock::data_bits() const {
  return data_;
}

ServiceMessage::ServiceMessage() {
}

void ServiceMessage::push_block(const SechBlock& block) {
  if (block.block_num() == 0)
    blocks_.clear();

  blocks_.push_back(block);
}

bool ServiceMessage::is_complete() const {
  bool is_complete = false;

  if (blocks_.empty()) {
    is_complete = false;
  } else if (blocks_.size() == 1) {
    is_complete = (blocks_[0].block_num() == 0 &&
                   blocks_[0].is_last_fragment());
  } else {
    is_complete = (blocks_[0].block_num() == 0 &&
                   blocks_.back().is_last_fragment());
    for (size_t i = 1; i < blocks_.size(); i++) {
      if (!blocks_[i].follows_in_sequence(blocks_[i - 1]))
        is_complete = false;
    }
  }

  return is_complete;
}

std::vector<int> ServiceMessage::data_bits() const {
  std::vector<int> result;
  for (SechBlock block : blocks_) {
    for (int b : block.data_bits())
      result.push_back(b);
  }

  return result;
}

void ServiceMessage::print(std::unique_ptr<Json::StreamWriter>& writer) const {
  if (!is_complete())
    return;

  Json::Value json;

  //printf("Service Message (%ld blocks): ", blocks_.size());

  int country_id = blocks_[0].country_id();

  std::vector<std::string> sech_types(
      {"COT", "AFT", "SAFT", "TDPNT", "SNT", "TDT", "SCOT"});
  //printf("CID:%d NID:%d ",
  //    blocks_[0].country_id(), blocks_[0].network_id());
  //printf("TYPE: %s",
  //    blocks_[0].data_type() < (int)sech_types.size() ? sech_types[blocks_[0].data_type()].c_str() : "err");

  std::vector<int> data = data_bits();

  json["cid"] = blocks_[0].country_id();
  json["nid"] = blocks_[0].network_id();

  int ecc = field(data, 0, 8);
  int tseid = field(data, 8, 7);
  int message_len = field(data, 15, 9);
  json["service_message"]["ecc"] = ecc;
  json["service_message"]["tse_id"] = tseid;
  //printf(" ecc:%d  tseid:%d  len:%d ", ecc, tseid, message_len);

  if (blocks_[0].data_type() == 0x5) {
    std::vector<int> time_bits(data.begin() + 3*8, data.begin() + 7*8 + 1);
    std::vector<int> date_bits(data.begin() + 7*8, data.begin() + 10*8 + 1);

    /*bool eta = field(time_bits, 0, 1);
    int hours = field(time_bits, 1, 5);
    int minutes = field(time_bits, 6, 6);
    int seconds = field(time_bits, 12, 6);
    int lto = field(time_bits, 18, 6);*/

    int name_len = field(date_bits, 2*8 + 2, 4);
    std::string name;
    for (int i = 0; i < name_len; i++) {
      char c = field(data, (10 + i) * 8, 8);
      name += c;
    }
    json["service_message"]["network_name"] = name;
    //json["service_message"]["time"] = TimeString(hours, minutes, seconds);

    bool has_position = field(date_bits, 2*9 + 6, 1);

    if (has_position) {

    }
  }

  std::stringstream ss;
  writer->write(json, &ss);
  ss << '\n';

  std::cout << ss.str() << std::flush;
}

Layer3::Layer3() : writer_builder_(), json_() {
  writer_builder_["indentation"] = "";
  writer_ =
      std::unique_ptr<Json::StreamWriter>(writer_builder_.newStreamWriter());
}

Layer3::~Layer3() {
}

void Layer3::push_block(const L2Block& l2block) {
  std::vector<int> info_bits = l2block.information_bits();
  std::vector<int> header(info_bits.begin(), info_bits.begin() + 24);
  std::vector<int> data(info_bits.begin() + 24, info_bits.end());

  uint16_t silch = field(header, 0, 4);

  if (silch == 0x8) {
    //printf("SeCH ");
    SechBlock sechblock(l2block.information_bits());
    service_message_.push_block(SechBlock(l2block.information_bits()));

    if (service_message_.is_complete()) {
      json_.clear();
      service_message_.print(writer_);
    }

  } else if (silch == 0x9) {
      //printf("SMCh ");
  } else if (silch == 0xA) {
      //printf("LMCh ");
  } else if (silch == 0xB) {
      //printf("BMCh ");
      bool is_realtime = field(header, 4, 1);
      int subchannel = field(header, 5, 3);
      //printf("subch:%d ", subchannel);
  } else {

  }
  //printf("\n");

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
