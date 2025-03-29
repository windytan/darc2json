/*
 * darc2json - DARC decoder
 * Copyright (c) Oona Räisänen OH2EIQ (windyoona@gmail.com)
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
#include <getopt.h>
#include <iostream>

#include "src/common.h"
#include "src/layer1.h"
#include "src/layer2.h"
#include "src/layer3_4.h"

namespace darc2json {

void PrintUsage() {
  std::cout << "radio_command | darc2json [OPTIONS]\n"
               "\n"
               "By default, a 228 kHz single-channel 16-bit MPX signal is expected via\n"
               "stdin.\n"
               "\n"
               "-e, --feed-through     Echo the input signal to stdout and print\n"
               "                       decoded groups to stderr.\n"
               "\n"
               "-E, --bler             Display the average block error rate, or the\n"
               "                       percentage of blocks that had errors before\n"
               "                       error correction. Averaged over the last 12\n"
               "                       groups. For hex input, this is the percentage\n"
               "                       of missing blocks.\n"
               "\n"
               "-f, --file FILENAME    Use an audio file as MPX input. All formats\n"
               "                       readable by libsndfile should work.\n"
               "\n"
               "-r, --samplerate RATE  Set stdin sample frequency in Hz. Will resample\n"
               "                       (slow) if this differs from 171000 Hz.\n"
               "\n"
               "-t, --timestamp FORMAT Add time of decoding to JSON groups; see\n"
               "                       man strftime for formatting options (or\n"
               "                       try \"%c\").\n"
               "\n"
               "-v, --version          Print version string.\n";
}

void PrintVersion() {
  std::cout << "darc2json 0.0.1-SNAPSHOT by OH2EIQ" << '\n';
}

Options GetOptions(int argc, char** argv) {
  darc2json::Options options;

  static struct option long_options[] = {
      {"feed-through", no_argument, 0, 'e'},
      {"bler",         no_argument, 0, 'E'},
      {"file",         1,           0, 'f'},
      {"samplerate",   1,           0, 'r'},
      {"timestamp",    1,           0, 't'},
      {"version",      no_argument, 0, 'v'},
      {"help",         no_argument, 0, '?'},
      {0,              0,           0, 0  }
  };

  int option_index = 0;
  int option_char;

  while ((option_char = getopt_long(argc, argv, "eEf:r:t:v", long_options, &option_index)) >= 0) {
    switch (option_char) {
      case 'e': options.feed_thru = true; break;
      case 'E': options.bler = true; break;
      case 'f':
#ifdef HAVE_SNDFILE
        options.sndfilename = std::string(optarg);
        options.input_type  = darc2json::INPUT_MPX_SNDFILE;
#else
        std::cerr << "error: darc2json was compiled without libsndfile" << '\n';
        options.just_exit = true;
#endif
        break;
      case 'p': options.show_partial = true; break;
      case 'r':
        options.samplerate = std::atoi(optarg);
        if (options.samplerate < 180000.f) {
          std::cerr << "error: sample rate must be 180000 Hz or higher" << '\n';
          options.just_exit = true;
        }
        break;
      case 't':
        options.timestamp   = true;
        options.time_format = std::string(optarg);
        break;
      case 'v':
        PrintVersion();
        options.just_exit = true;
        break;
      case '?':
      default:
        PrintUsage();
        options.just_exit = true;
        break;
    }
    if (options.just_exit)
      break;
  }

  if (options.feed_thru && options.input_type == INPUT_MPX_SNDFILE) {
    std::cerr << "error: feed-thru is not supported for audio file inputs" << '\n';
    options.just_exit = true;
  }

  return options;
}

}  // namespace darc2json

int main(int argc, char** argv) {
  darc2json::Options options = darc2json::GetOptions(argc, argv);

  if (options.just_exit)
    return EXIT_FAILURE;

  darc2json::Layer2 layer2;
  darc2json::Layer3 layer3(options);

  darc2json::Subcarrier subc(options);
  while (!subc.eof()) {
    std::vector<darc2json::L2Block> blocks = layer2.PushBit(subc.NextBit());
    for (darc2json::L2Block l2block : blocks) {
      layer3.push_block(l2block);
    }
  }

  return EXIT_SUCCESS;
}
