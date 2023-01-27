#!/bin/bash

#exit an any error
set -e

function get_config_var()
{
	cat config.json | grep "\"$1\"" | sed -e 's/.*": "//' -e 's/".*//'
}

TARGET=$(get_config_var target)

mbed compile --profile ./mbed-os/tools/profiles/debug.json
st-flash --reset --flash=128k write BUILD/$TARGET/GCC_ARM-DEBUG/*.bin 0x8000000
type -P pyocd > /dev/null && pyocd gdbserver -p 4242 || st-util -u
