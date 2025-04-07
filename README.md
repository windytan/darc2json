# darc2json

darc2json is a proof-of-concept DARC demodulator for Linux/macOS. It takes an
FM multiplex signal as input, in raw PCM format, and outputs line-delimited
JSON messages. It was designed to be used with the RTL-SDR tool `rtl_fm`, but
it can read any FM multiplex signals.

The default input sample rate is 228 kHz.

## Supported features

darc2json can decode:

* L5 Group Data (type 12)
* Raw Layer 4 LMCh data if type is something else
* Block application channel: Layer 3 data
* TDT: Country code, network name, date and time

## Not implemented

A list of things to fix for your own implementation:

* No PLL and symbol synchronization (amazingly, it kind of works)
* No error correction beyond single bit flips
* No Fragmented L5
* No Short message channel
* No Synchronous Frame Messages
* No COT, SCOT, AFT, SAFT
* No Conditional Access at L4
* Drops block sync at first error
* Needs more allocation-efficient handling of bitstrings
* Repeats unchanged service messages

## Installation

You will need git, a C++17 compiler, the [liquid-dsp][liquid-dsp] library, libsndfile, and meson.
On macOS (OSX) you will also need XCode command-line tools (`xcode-select --install`).

1. Clone the repository (unless you downloaded a release zip file):

        $ git clone https://github.com/windytan/darc2json.git
        $ cd darc2json

2. Compile darc2json:

        $ meson setup build
        $ cd build
        $ meson compile

3. Install:

        $ meson install

It is also simple to later pull the latest updates and recompile:

        $ git pull
        $ cd build
        $ meson compile
        $ meson install

[liquid-dsp]: https://github.com/jgaeddert/liquid-dsp

## Usage

The simplest way to view DARC messages using `rtl_fm` is:

    rtl_fm -M fm -l 0 -A std -p 0 -s 228k -g 20 -F 9 -f 87.9M | darc2json

### Full usage

```
radio_command | darc2json [OPTIONS]

By default, a 228 kHz single-channel 16-bit MPX signal is expected via
stdin.

-f, --file FILENAME    Use an audio file as MPX input. All formats
                       readable by libsndfile should work.

-r, --samplerate RATE  Set stdin sample frequency in Hz. Will resample
                       (slow) if this differs from 228000 Hz.

-t, --timestamp FORMAT Add time of decoding to JSON groups; see
                       man strftime for formatting options (or
                       try "%c").

-v, --version          Print version string.
```

## Troubleshooting

### Can't find liquid-dsp on macOS

If you've installed [liquid-dsp][liquid-dsp] yet `configure` can't find it, it's
possible that XCode command line tools aren't installed. Run this command to fix
it:

    xcode-select --install

### Can't find liquid-dsp on Linux

Try running this in the terminal:

    sudo ldconfig

## Licensing

See [LICENSE](LICENSE).
