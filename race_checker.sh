#!/bin/sh

# pro=bodytrack
# ~/parsec-2.1/bin/parsecmgmt -a run -p $pro -c gcc-pthreads -i simmedium -n 4 -s "\
/home/yiranyaoqiu/pin/pin -t /home/yiranyaoqiu/RaceChecker/build-debug/race_profiler.so \
-ignore_lib 1 -enable_literace 1 -track_racy_inst 1 \
-enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 -debug_pthread 1 -debug_malloc 1 --\
 test/unittest
# -- ~/splash2_origin/codes/kernels/cholesky/CHOLESKY -p32 -B32 -C16384 \
# ~/splash2_origin/codes/kernels/cholesky/inputs/tk17.O 