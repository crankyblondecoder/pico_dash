#!/bin/bash

# Full cmake rebuild.

cd ./build/release

rm -rf *

# This is absolutely vital otherwise cmake _doesn't_ use the cross compiler!
export PICO_SDK_PATH=/home/stodas/devel/pico/pico-sdk/

cmake -DCMAKE_BUILD_TYPE=Release ../../src/

