#!/bin/bash

arm-none-eabi-gdb -ex "target remote localhost:3333" -ex "monitor reset init" ./build/debug/farm_box_pico.elf
