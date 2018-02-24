ESP32 Soundboard
=======================
Press a button, play a sound.

## How To Use It

Configure and flash. Press the GPIO0 button on your dev board.

## Downloading Required Software

Get the SDK:

    git clone https://github.com/espressif/esp-idf.git
    cd esp-idf
    git submodule update --init

Set the IDF_PATH environment variable, and point it to this directory.

    export IDF_PATH=/path/to/esp-idf

Download the toolchain from: https://github.com/espressif/esp-idf#setting-up-esp-idf
You will need the newest version.
Add /path/to/xtensa-esp32-elf/bin to your PATH:

    export PATH=/path/to/xtensa-esp32-elf/bin:$PATH

## Building

Execute 'make menuconfig' and configure your serial port, leave the rest at default settings and then execute 'make flash'.

Build the SPIFFS image with `make makefs`, flash it with `make flashfs`.

## Connecting the I2S codec

If you don't know about the I2S standard, it is a special protocol for transferring digital audio data between chips, similar to I2C. There are many I2S chips you can choose from, the most important differences are:

1. Amplification: some chips only decode the audio to a low analog level, so you need a separate amp, but some also have a built-in amplifier. Most of these 2-in-1 chips are made for smartphones so their energy output is in the range of 2-4W, but some other ones made for domestic audio appliances can go a lot higher.
2. MCLK: this is a separate clock signal that sometimes needs to be a precise number in the MHz range that depends on the current sample rate, sometimes can be a single constant value ("asynchronous") independent of the current sample rate, and sometimes is not required at all. The ESP32 does not output a MCLK signal, so a chip that does not require MCLK is most convenient. If you already have an asynchronous one lying around (e.g. ES9023), you will need a quartz oscillator, usually in the range of 20-50MHz.

I tested several I2S codecs, and was happiest with the MAX98357A, because it does not require MCLCK and also amplifies the audio to speaker levels. It also seemed to be more immune to signal integrity issues, which do occur on breadboards. There is a convenient breakout board from Adafruit: https://www.adafruit.com/product/3006
However, any I2S codec should work.

Generic wiring:

```
ESP pin   - I2S signal
----------------------
GPIO25/DAC1   - LRCK
GPIO26/DAC2   - BCLK
GPIO22        - DATA
```

If you're using the MAX98357A, connect GND to ground and Vin to +5V (or +3.3V if +5V is unavailable). SD can remain unconnected, and GAIN too unless you want to make it louder or lower. I also recommend using a potentiometer for volume regulation.

## Sounds
There a pre-loaded ones, taken from https://www.youtube.com/watch?v=BlEJGwQgkgY.

You can add custom sounds in components/spiffs_image/image. Format should be 16 bit stereo 44.1kHz, raw.

## License
Mozilla Public License 2.0. Here is a summary of what this means: https://choosealicense.com/licenses/

