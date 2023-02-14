Notes
=====
- This code is made to be modified by you! This is a very simple runtime 
  that might need modifications to suite your needs.
- Fastest raster can be 1ms (1ms clock) or async.
- Saving parameters is not supported. After reset they have
  the default value
- Some Serial to USB convertes have a higer latency. You can try
  'echo 1 > /sys/bus/usb-serial/devices/ttyUSB0/latency_timer'

Installation
============
- Go to workspace

  - git clone https://github.com/abmt-community/rt_mbed.git
  - git clone https://github.com/abmt-community/mbed_nodes.git

- sudo apt-get install pip wget openocd zip
- pip install mbed-cli jinja2 pyyaml jsonschema mbed_ls mbed_host_tests mbed_greentea pyelftools pycryptodome pyusb cmsis_pack_manager psutil cryptography click cbor
- restart console for PATH update
- Download gcc 9.x from https://developer.arm.com/downloads/-/gnu-rm and remember folder for toolchain path

MBED sources
============
- https://api.github.com/repos/ARMmbed/mbed-os/tarball/mbed-os-6.16.0
- https://github.com/arduino/mbed-os/archive/refs/heads/extrapatches-6.16.0.zip

Debug
=====
- There are many ways, this is only one.
- Go to build-directory
- build with debug options 'mbed compile --profile ./mbed-os/tools/profiles/debug.json'
- Flash the new binary (in 'BUILD/TARGET/TOOLCHAIN-DEBUG/some_name.bin')
- Start debugger ('st-util' or 'pyocd')
- Open eclipse:
  - Create new C++ Project in the build directory
  - Create new Debug configuration
    - Select Binary
    - Select gdb-port
- Debug...

Tips
-----
- pyocd can decode threads
- The build-directory contains a debug.sh that might help
- mbeds hard fault handler has a blinking pattern. You could try to increase the stack size.
- You can use pyocd to flash. STM32F103 need a full chip erase. ``pyocd flash --target stm32f103rc -e chip path/to/bin_hex_or_elf``
- When mbed crashes it dumps some info on the serial port (Pins: CONSOLE_RX/-TX).

Raspbery Pi Pico Notes
======================
- mbed port not finished yet (Feb 23)...
- mbed: https://github.com/arduino/mbed-os/archive/refs/heads/extrapatches-6.16.0.zip
- Target: RASPBERRY_PI_PICO
- elf2uf2: https://github.com/rej696/elf2uf2
- tip: enable auto-mount

Example flash commands
----------------------
- pyocd flash BUILD/$TARGET/GCC_ARM/*.elf -t rp2040
- pyocd flash BUILD/$TARGET/GCC_ARM/*.elf -e chip -t stm32f103rc
- st-flash --reset --flash=128k write BUILD/$TARGET/GCC_ARM/*.bin 0x8000000
- stm32flash -b 576000 -w BUILD/$TARGET/GCC_ARM/*.bin -v -g 0x0 /dev/ttyUSB0
- test -f /media/<username>/RPI-RP2/INFO_UF2.TXT && /path_to_elf2uf2/elf2uf2 BUILD/$TARGET/GCC_ARM/*.elf /media/<username>/RPI-RP2/mbed.uf2

Konwn Issues
============
- Don't use header files with names that match standard header files (eg "math.h")
  your nodes. MBED would shadow the default ones.
- There are some deprecated warnings regarding the need to use std::chrono, which is not
  used for compability reasons with mbed5. Unfortunatly mbed6 droped the support for a lot
  of easy accessible hardware...
