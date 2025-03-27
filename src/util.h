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
#ifndef UTIL_H_
#define UTIL_H_

#include <map>
#include <string>
#include <vector>
#include <cstdint>

namespace darc2json {

using Bits = std::vector<uint8_t>;
using Bytes = std::vector<uint8_t>;

Bits poly_coeffs_to_bits(const std::vector<int>& coeffs);
Bits bitvector_lsb(std::vector<uint8_t> input);
Bits bitvector_msb(std::vector<uint8_t> input);
uint32_t field(const Bits& bits,
               int start_at, int length);
uint32_t field_rev(const Bits& bits,
                   int start_at, int length);
void lshift(Bits& bits);
//Bits crc(Bits bits, const Bits& generator);
bool BitsEqual(const Bits& bits1, const Bits& bits2);
std::string BitString(const Bits& bits);
Bits crc(const Bits& bits, const Bits& generator, size_t message_length);

bool check_crc(const Bits& bits, const Bits& generator, size_t message_length);

const std::map<Bits, Bits> create_bitflip_syndrome_map(size_t len,
                                                       const Bits& generator);
std::string BitsToHexString(const Bits& data);
std::string BytesToHexString(const std::vector<uint8_t>& data);

bool AllBitsZero(const Bits& bits);

Bits reversed_bytes_to_bit_vector(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> bit_vector_to_reversed_bytes(const Bits& bits);

uint32_t bfield(const std::vector<uint8_t>& bytes, size_t start_byte,
                size_t start_bit, size_t length);

}  // namespace darc2json
#endif  // UTIL_H_
