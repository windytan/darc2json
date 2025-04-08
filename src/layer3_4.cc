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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>

#include "src/layer2.h"
#include "src/util.h"

namespace darc2json {

const Bits kL3ShortMessageHeaderCRC = poly_coeffs_to_bits({6, 4, 3, 0});
const Bits kL4ShortMessageHeaderCRC = poly_coeffs_to_bits({8, 5, 4, 3, 0});
const Bits kL3LongMessageHeaderCRC  = poly_coeffs_to_bits({6, 4, 3, 0});
const Bits kL4LongMessageHeaderCRC  = poly_coeffs_to_bits({6, 4, 3, 0});

std::string TimeString(int hours, int minutes, int seconds) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << hours << ":" << minutes << ":" << seconds;
  return ss.str();
}

std::string TimePointToString(const std::chrono::time_point<std::chrono::system_clock>& timepoint,
                              const std::string& format) {
  std::time_t t = std::chrono::system_clock::to_time_t(timepoint);
  std::tm tm    = *std::localtime(&t);

  std::string result;
  char buffer[64];
  if (strftime(buffer, sizeof(buffer), format.c_str(), &tm) > 0) {
    result = std::string(buffer);
  } else {
    result = "(format error)";
  }

  return result;
}

SechBlock::SechBlock(const Bits& info_bits)
    : is_last_fragment_(field(info_bits, 5, 1)),
      data_update_(field(info_bits, 6, 2)),
      country_id_(field(info_bits, 8, 4)),
      data_type_(field(info_bits, 12, 4)),
      network_id_(field(info_bits, 16, 4)),
      block_num_(field(info_bits, 20, 4)),
      data_(info_bits.begin() + 24, info_bits.begin() + 24 + 19 * 8) {}

bool SechBlock::is_last_fragment() const {
  return is_last_fragment_;
}

int SechBlock::block_num() const {
  return block_num_;
}

bool SechBlock::follows_in_sequence(const SechBlock& previous) const {
  return data_update_ == previous.data_update_ && country_id_ == previous.country_id_ &&
         data_type_ == previous.data_type_ && network_id_ == previous.network_id_ &&
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
    for (int b : block.data_bits()) result.push_back(b);
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

  const std::vector<std::string> sech_types({"COT", "AFT", "SAFT", "TDPNT", "SNT", "TDT", "SCOT"});

  const Bits data = data_bits();

  const std::vector<std::uint8_t> data_bytes = bit_vector_to_reversed_bytes(data);

  json["country_code"] = country_id();
  json["network_id"]   = network_id();

  json["service_message"]["type"] =
      (data_type() < static_cast<int>(sech_types.size()) ? sech_types.at(data_type()) : "err");

  const int ecc                     = field(data, 0, 8);
  const int tseid                   = field(data, 8, 7);
  // int message_len = field(data, 15, 9);
  json["service_message"]["tse_id"] = tseid;

  json["service_message"]["country"] = CountryString(country_id(), ecc);

  if (data_type() == kTypeTDT) {
    const Bits time_bits(data.begin() + 3 * 8, data.begin() + 7 * 8 + 1);
    const Bits date_bits(data.begin() + 7 * 8, data.begin() + 10 * 8 + 1);

    int modified_julian_date = bfield(data_bytes, 7, 6, 17);

    const double local_offset =
        (bfield(data_bytes, 5, 5, 1) ? -1 : 1) * bfield(data_bytes, 5, 4, 5) / 2.0;
    modified_julian_date += local_offset / 24.0;

    int year  = (modified_julian_date - 15078.2) / 365.25;
    int month = (modified_julian_date - 14956.1 - std::trunc(year * 365.25)) / 30.6001;
    const int day =
        modified_julian_date - 14956 - std::trunc(year * 365.25) - std::trunc(month * 30.6001);
    if (month == 14 || month == 15) {
      year += 1;
      month -= 12;
    }
    year += 1900;
    month -= 1;

    const int local_offset_min = (local_offset - std::trunc(local_offset)) * 60;

    const bool eta    = bfield(data_bytes, 3, 7, 1);
    // const std::uint8_t taf = data_bytes[6];
    const int hour    = bfield(data_bytes, 3, 6, 5);
    const int minute  = bfield(data_bytes, 3, 1, 6);
    const int seconds = bfield(data_bytes, 4, 3, 6);

    const bool is_date_valid =
        (month >= 1 && month <= 12 && day >= 1 && day <= 31 && hour >= 0 && hour <= 23 &&
         minute >= 0 && minute <= 59 && fabs(std::trunc(local_offset)) <= 14.0);
    if (is_date_valid) {
      char buffer[100];
      const int local_offset_hour = fabs(std::trunc(local_offset));

      if (local_offset_hour == 0 && local_offset_min == 0) {
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", year, month, day,
                      hour, minute, seconds);
      } else {
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d%s%02d:%02d", year,
                      month, day, hour, minute, seconds, local_offset > 0 ? "+" : "-",
                      local_offset_hour, abs(local_offset_min));
      }
      json["service_message"]["clock_time"] = std::string(buffer);
    }

    size_t name_len = bfield(data_bytes, 9, 5, 4);
    std::string name;
    if (data_bytes.size() >= 10 + name_len - 1) {
      for (size_t i = 0; i < name_len; i++) {
        name += (char)(data_bytes[10 + i]);
      }
    }
    json["service_message"]["network_name"]  = name;
    json["service_message"]["time_accurate"] = eta;

    bool has_position = field(date_bits, 2 * 9 + 6, 1);

    if (has_position) {
    }
  }

  return json;
}

LongBlock::LongBlock(const Bits& info_bits)
    : is_last_fragment_(field(info_bits, 5, 1)),
      sequence_counter_(field(info_bits, 6, 4)),
      bytes_(bit_vector_to_reversed_bytes(Bits(info_bits.begin() + 16, info_bits.end()))) {
  // bool di = field(info_bits, 4, 1);
  l3_header_crc_ok_ = check_crc(info_bits, kL3LongMessageHeaderCRC, 16);

  /*  printf(" LF[%s] SC:%02d L3_CRC_OK[%s]\n",
        is_last_fragment_ ? "x" : " ",
        sequence_counter_,
        l3_header_crc_ok_ ? "x" : " ");*/
}

bool LongBlock::is_last_fragment() const {
  return is_last_fragment_;
}

bool LongBlock::header_crc_ok() const {
  return l3_header_crc_ok_;
}

bool LongBlock::follows_in_sequence(const LongBlock& previous) const {
  return sequence_counter_ == ((previous.sequence_counter_ + 1) % 16) && header_crc_ok() &&
         previous.header_crc_ok() && !previous.is_last_fragment();
}

Bits LongBlock::data() const {
  return bytes_;
}

LongMessage::LongMessage() : is_complete_(false), l4_header_crc_ok_(false) {}

void LongMessage::push_block(const LongBlock& block) {
  if (blocks_.size() > 0 && !block.follows_in_sequence(blocks_.back())) {
    blocks_.clear();
    bytes_.clear();
    l4_header_crc_ok_ = false;
    is_complete_      = false;
  }

  if (block.header_crc_ok()) {
    blocks_.push_back(block);
    for (const std::uint8_t byte : block.data()) bytes_.push_back(byte);

    if (block.is_last_fragment())
      parse_l4_header();
  }
}

void LongMessage::parse_l4_header() {
  l4_header_crc_ok_ = false;

  if (blocks_.empty())
    return;

  Bits header_bits = reversed_bytes_to_bit_vector(bytes_);

  // const int ci   = field(header_bits, 2, 2);
  const int fl   = field(header_bits, 4, 2);
  const bool ext = field(header_bits, 6, 1);
  const bool caf = field(header_bits, 17, 1);
  // size_t dlen = field(header_bits, 18 + ext * 8, 8);

  is_first_ = fl & 1;
  is_last_  = fl & 2;

  if (caf)
    return;

  header_bits.resize((4 + ext) * 8);

  const bool crc_ok   = check_crc(header_bits, kL4LongMessageHeaderCRC, header_bits.size());
  const bool complete = true;  //(bytes_.size() >= dlen);

  /*printf("L4: CI:%d ext:[%s] caf:[%s] dlen:%3ld "
         "(rx: %3ld) L4_CRC_OK[%s]",
      ci,
      ext ? "x" : " ",
      caf ? "x" : " ",
      dlen,
      bytes_.size(),
      crc_ok ? "x" : " ");
  if (complete)
    printf(" COMPLETE");

  printf("\n");*/
  if (crc_ok && complete) {
    is_complete_ = true;
    // bytes_.resize(dlen);
    /*printf("data: ");
    for (uint8_t c : bytes_)
      printf("%02x ", c);
    for (uint8_t c : bytes_)
      printf("%c", c);*/
  }
  // printf("\n\n");
}

bool LongMessage::is_complete() const {
  return is_complete_;
}

Json::Value LongMessage::to_json() const {
  Json::Value json;

  json["long_message"]["first"] = is_first_;
  json["long_message"]["last"]  = is_last_;

  if (is_first_ && is_last_) {
    const bool has_crc              = (bytes_[0] >> 6) & 1;
    const int type                  = (bytes_[0]) & 0xf;
    json["long_message"]["has_crc"] = has_crc;
    json["long_message"]["type"]    = type;
    // printf("lm:%s\n",BytesToHexString(bytes_).c_str());
    if (type == 12) {
      const int transport_id               = bfield(bytes_, 3, 0, 16);
      const int len                        = bfield(bytes_, 2, 7, 4);  // ibytes_[2] & 0xf;
      json["long_message"]["transport_id"] = transport_id;
      json["long_message"]["hlen"]         = len;

      std::size_t nbyte = 5;
      while (nbyte < bytes_.size() - 1) {
        // std::uint8_t tag = bytes_[nbyte];
        nbyte++;
        const std::uint8_t len = bytes_[nbyte];
        nbyte++;
        if (len > 0) {
          std::vector<uint8_t> tlv_bytes;
          for (int l = 0; l < len && nbyte < bytes_.size(); l++) {
            tlv_bytes.push_back(bytes_[nbyte]);
            nbyte++;
          }
          Json::Value group_data(BytesToHexString(tlv_bytes));
          if (!tlv_bytes.empty())
            json["long_message"]["group_data"].append(group_data);
        }
      }
    } else {
      json["long_message"]["l4data"] = BytesToHexString(bytes_);
    }
  } else {
    json["long_message"]["l4data"] = BytesToHexString(bytes_);
  }

  return json;
}

Layer3::Layer3(const Options& options) : options_(options), writer_builder_() {
  writer_builder_["indentation"] = "";
  writer_ = std::unique_ptr<Json::StreamWriter>(writer_builder_.newStreamWriter());
}

Layer3::~Layer3() {}

void Layer3::push_block(const L2Block& l2block) {
  const Bits info_bits = l2block.information_bits();

  const std::uint16_t silch = field(info_bits, 0, 4);

  if (silch == 0x8) {
    service_message_.push_block(SechBlock(info_bits));

    if (service_message_.is_complete())
      print_line(service_message_.to_json());

  } else if (silch == 0x9) {
    /*printf("SMCh ");
    short_message_.push_block(ShortBlock(info_bits));

    if (short_message_.is_complete())
      print_line(short_message_.to_json());*/

  } else if (silch == 0xA) {
    long_message_.push_block(LongBlock(info_bits));

    if (long_message_.is_complete())
      print_line(long_message_.to_json());

  } else if (silch == 0xB) {
    // bool is_realtime = field(info_bits, 4, 1);
    int subchannel = field(info_bits, 5, 3);
    if (subchannel == 0x0) {
      const Bits data(info_bits.begin() + 8, info_bits.end());
      Json::Value json;

      json["block_app"]["l3data"] = BitsToHexString(data);
      print_line(json);
    }
    // printf("subch:%d ", subchannel);
  } else {
  }
}

void Layer3::print_line(Json::Value json) {
  if (options_.timestamp)
    json["rx_time"] = TimePointToString(std::chrono::system_clock::now(), options_.time_format);
  std::stringstream ss;
  writer_->write(json, &ss);
  ss << '\n';
  std::cout << ss.str() << std::flush;
}

// EN 50067:1998, Annex D, Table D.1 (p. 71)
// RDS Forum R08/008_7, Table D.2 (p. 75)
std::string CountryString(std::uint16_t cid, std::uint16_t ecc) {
  static const std::map<std::uint16_t, std::vector<std::string>> country_codes({
      {0xA0,
       {"us", "us", "us", "us", "us", "us", "us", "us", "us", "us", "us", "--", "us", "us", "--"}},
      {0xA1,
       {"--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "ca", "ca", "ca", "ca", "gl"}},
      {0xA2,
       {"ai", "ag", "ec", "fk", "bb", "bz", "ky", "cr", "cu", "ar", "br", "bm", "an", "gp", "bs"}},
      {0xA3,
       {"bo", "co", "jm", "mq", "gf", "py", "ni", "--", "pa", "dm", "do", "cl", "gd", "tc", "gy"}},
      {0xA4,
       {"gt", "hn", "aw", "--", "ms", "tt", "pe", "sr", "uy", "kn", "lc", "sv", "ht", "ve", "--"}},
      {0xA5,
       {"--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "mx", "vc", "mx", "mx", "mx"}},
      {0xA6,
       {"--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "pm"}},
      {0xD0,
       {"cm", "cf", "dj", "mg", "ml", "ao", "gq", "ga", "gn", "za", "bf", "cg", "tg", "bj", "mw"}},
      {0xD1,
       {"na", "lr", "gh", "mr", "st", "cv", "sn", "gm", "bi", "--", "bw", "km", "tz", "et", "bg"}},
      {0xD2,
       {"sl", "zw", "mz", "ug", "sz", "ke", "so", "ne", "td", "gw", "zr", "ci", "tz", "zm", "--"}},
      {0xD3,
       {"--", "--", "eh", "--", "rw", "ls", "--", "sc", "--", "mu", "--", "sd", "--", "--", "--"}},
      {0xE0,
       {"de", "dz", "ad", "il", "it", "be", "ru", "ps", "al", "at", "hu", "mt", "de", "--", "eg"}},
      {0xE1,
       {"gr", "cy", "sm", "ch", "jo", "fi", "lu", "bg", "dk", "gi", "iq", "gb", "ly", "ro", "fr"}},
      {0xE2,
       {"ma", "cz", "pl", "va", "sk", "sy", "tn", "--", "li", "is", "mc", "lt", "rs", "es", "no"}},
      {0xE3,
       {"me", "ie", "tr", "mk", "--", "--", "--", "nl", "lv", "lb", "az", "hr", "kz", "se", "by"}},
      {0xE4,
       {"md", "ee", "kg", "--", "--", "ua", "ks", "pt", "si", "am", "--", "ge", "--", "--", "ba"}},
      {0xF0,
       {"au", "au", "au", "au", "au", "au", "au", "au", "sa", "af", "mm", "cn", "kp", "bh", "my"}},
      {0xF1,
       {"ki", "bt", "bd", "pk", "fj", "om", "nr", "ir", "nz", "sb", "bn", "lk", "tw", "kr", "hk"}},
      {0xF2,
       {"kw", "qa", "kh", "ws", "in", "mo", "vn", "ph", "jp", "sg", "mv", "id", "ae", "np", "vu"}},
      {0xF3,
       {"la", "th", "to", "--", "--", "--", "--", "--", "pg", "--", "ye", "--", "--", "fm",
        "mn"}                                                                                    }
  });

  std::string result("--");

  if (country_codes.find(ecc) != country_codes.end() && cid > 0) {
    result = country_codes.at(ecc).at(cid - 1);
  }

  return result;
}

}  // namespace darc2json
