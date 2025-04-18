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
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

namespace darc2json {

// Convert generator polynomial (x^6 + x^4 + x^3 + 1) coefficients
// ({6, 4, 3, 0}) to bitstring ({1, 0, 1, 1, 0, 0, 1})
Bits poly_coeffs_to_bits(const std::initializer_list<int>& coeffs) {
  assert(!std::empty(coeffs));
  Bits bits(*coeffs.begin() + 1);
  for (const int c : coeffs) bits.at(bits.size() - c - 1) = 1;

  return bits;
}

Bits bitvector_lsb(const std::vector<uint8_t>& input) {
  Bits result;

  for (const std::uint8_t c : input) {
    for (int i = 7; i >= 0; i--) {
      result.push_back((c >> i) & 1);
    }
  }
  return result;
}

Bits bitvector_msb(const std::vector<std::uint8_t>& input) {
  Bits result;

  for (const std::uint8_t c : input) {
    for (int i = 0; i < 8; i++) {
      result.push_back((c >> i) & 1);
    }
  }
  return result;
}

// \return Value of a bit field
// \param start_at First bit, counting from LSB = 0
// \param length Number of bits in the field
std::uint32_t field(const Bits& bits, int start_at, int length) {
  assert(length <= 32);
  assert(start_at + length <= static_cast<int>(bits.size()));
  assert(start_at >= 0);
  std::uint32_t result = 0;
  for (int i = 0; i < length; i++) {
    result += (bits.at(start_at + i) << i);
  }

  return result;
}

std::uint32_t field_rev(const Bits& bits, int start_at, int length) {
  assert(length <= 32);
  assert(start_at + length <= static_cast<int>(bits.size()));
  assert(start_at >= 0);
  std::uint32_t result = 0;
  for (int i = 0; i < length; i++) {
    result += (bits.at(start_at + i) << (length - 1 - i));
  }

  return result;
}

// Shift bits to the left by one position
void lshift(Bits& bits) {
  assert(!std::empty(bits));
  for (int i = 0; i < static_cast<int>(bits.size()) - 1; i++) bits[i] = bits[i + 1];
  bits[bits.size() - 1] = 0;
}

Bits crc(const Bits& bits, const Bits& generator, std::size_t message_length) {
  assert(message_length <= bits.size());
  assert(generator.size() > 1);
  Bits result(generator.size() - 1);

  assert(!std::empty(result));
  assert(generator.size() > result.size());
  for (std::size_t n_bit = 0; n_bit < message_length; n_bit++) {
    const int popped_bit = result[0];
    lshift(result);
    result[result.size() - 1] = bits[n_bit];

    // XOR if shifted-out bit was 1
    if (popped_bit) {
      for (std::size_t j = 0; j < result.size(); j++) result[j] ^= generator[j + 1];
    }
  }

  return result;
}

// Zero-pad data-only message and return calculated CRC
/*Bits crc(Bits bits, const Bits& generator) {
  for (size_t i = 0; i < generator.size() - 1; i++)
    bits.push_back(0);

  return _crc(bits, generator, bits.size());
}*/

bool check_crc(const Bits& bits, const Bits& generator, std::size_t message_length) {
  return AllBitsZero(crc(bits, generator, message_length));
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

// Bits to string (0010100101...)
std::string BitString(const Bits& bits) {
  std::string result;
  result.reserve(bits.size());
  for (const int b : bits) result += std::to_string(b);
  return result;
}

const std::map<Bits, Bits> create_bitflip_syndrome_map(std::size_t len, const Bits& generator) {
  std::map<Bits, Bits> result;

  for (std::size_t i = 0; i < len; i++) {
    Bits error_vector(len);
    error_vector.at(i) = 1;

    result[crc(error_vector, generator, error_vector.size())] = error_vector;
  }

  return result;
}

// Bytes to hex string (01 2c 52 00 ...)
std::string BytesToHexString(const std::vector<uint8_t>& data) {
  std::stringstream ss;
  for (std::size_t n_byte = 0; n_byte < data.size(); n_byte++) {
    ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(data[n_byte]);

    if (n_byte < data.size() - 1)
      ss << " ";
  }

  return ss.str();
}

std::string BitsToHexString(const Bits& data) {
  std::stringstream ss;
  for (size_t nbyte = 0; nbyte < data.size() / 8; nbyte++) {
    ss << std::setfill('0') << std::setw(2) << std::hex << field(data, nbyte * 8, 8);

    if (nbyte < data.size() / 8 - 1)
      ss << " ";
  }

  return ss.str();
}

bool AllBitsZero(const Bits& bits) {
  for (const auto bit : bits) {
    if (bit)
      return false;
  }

  return true;
}

Bits reversed_bytes_to_bit_vector(const std::vector<std::uint8_t>& bytes) {
  Bits bits;
  bits.reserve(bytes.size() * 8);
  for (const std::uint8_t byte : bytes)
    for (int n_bit = 0; n_bit < 8; n_bit++) bits.push_back((byte >> (7 - n_bit)) & 1);

  return bits;
}

std::vector<std::uint8_t> bit_vector_to_reversed_bytes(const Bits& bits) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(bits.size() / 8);
  for (size_t n_byte = 0; n_byte < bits.size() / 8; n_byte++) {
    bytes.push_back(field(bits, n_byte * 8, 8));
  }
  return bytes;
}

// Extract a field from a vector of bytes.
// The bit numbering in a byte corresponds to that used in the DARC
// specification.
std::uint32_t bfield(const std::vector<std::uint8_t>& bytes, std::size_t start_byte,
                     std::size_t start_bit, std::size_t length) {
  assert(start_byte < bytes.size());
  assert(length <= bytes.size() * 8);
  std::uint32_t result = 0;
  int n_byte           = start_byte;
  int n_bit            = start_bit;
  for (std::size_t n_bit_result = 0; n_bit_result < length; n_bit_result++) {
    result += ((bytes[n_byte] >> n_bit) & 1) << (length - n_bit_result - 1);
    n_bit--;
    if (n_bit < 0) {
      n_byte++;
      n_bit = 7;
    }
  }

  return result;
}

}  // namespace darc2json
