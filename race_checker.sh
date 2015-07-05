#!/bin/sh

# pro=facesim
# ~/parsec-2.1/bin/parsecmgmt -a run -p $pro -c gcc-pthreads -i simdev -n 4 -s " \

/home/yiranyaoqiu/pin/pin -t /home/yiranyaoqiu/RaceChecker/build-debug/race_profiler.so \
-ignore_lib 1 -enable_eraser 1 -track_racy_inst 1 \
-enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 -debug_pthread 1 -debug_malloc 1 \
-- ~/桌面/condvar