#!/bin/bash

st-flash --reset write bluepill_option_bytes.bin 0x1FFFF800
