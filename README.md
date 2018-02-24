# darc2json

darc2json is a DARC demodulator. It takes a 228 kHz FM multiplex signal as
input, in raw PCM format, and outputs line-delimited JSON messages. It was
designed to be used with the RTL-SDR tool `rtl_fm`, but it can read any FM
multiplex signals.

## Supported features

Currently, darc2json will only decode:

* Block application channel: Layer 3 data
* Long message channel: Layer 4 data
* TDT: Country code, network name, date and time

## TODO

* Short message channel
* Synchronous Frame Messages
* PLL and symbol synchronization
* Error correction beyond single bit flips
* More robust block synchronization?
* More efficient handling of bitstrings
* COT, SCOT, AFT, SAFT
* Conditional Access at L4
* Not repeating unchanged service messages

## Installation

You will need git, the [liquid-dsp][liquid-dsp] library, and GNU autotools.
Audio files can be decoded if libsndfile is installed. On macOS (OSX) you will
also need XCode command-line tools (`xcode-select --install`).

1. Clone the repository (unless you downloaded a release zip file):

        $ git clone https://github.com/windytan/darc2json.git
        $ cd darc2json

2. Compile darc2json:

        $ ./autogen.sh && ./configure && make

3. Install:

        $ make install

It is also simple to later pull the latest updates and recompile:

        $ git pull
        $ ./autogen.sh && ./configure && make clean && make
        $ make install

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

darc2json is released under the MIT license, which means it is copyrighted to
Oona Räisänen OH2EIQ yet you're free to use it provided that the copyright
information is not removed. (jsoncpp has its own license.) See
[LICENSE](LICENSE).
