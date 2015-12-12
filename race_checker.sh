#!/bin/sh

# pro=bodytrack
# ~/parsec-2.1/bin/parsecmgmt -a run -p $pro -c gcc-pthreads -i simmedium -n 4 -s "\
/home/yiranyaoqiu/pin/pin -t /home/yiranyaoqiu/RaceChecker/build-debug/race_profiler.so \
-ignore_lib 1 -enable_multilock_hb 1 -track_racy_inst 1 \
-static_profile static_profile/static_profile_13.out \
-instrumented_lines static_profile/instrumented_lines_13.out \
-loop_range_lines static_profile/loop_range_lines_13.out -race_verify_ml 1 \
-enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 -debug_pthread 1 -debug_malloc 1 -- \
test/verify/verifier13