#!/bin/sh

# pro=bodytrack
# ~/parsec-2.1/bin/parsecmgmt -a run -p $pro -c gcc-pthreads -i simmedium -n 4 -s "\
/home/yiranyaoqiu/pin/pin -t /home/yiranyaoqiu/RaceChecker/build-debug/race_profiler.so \
-ignore_lib 1  -enable_multilock_hb 1 -track_racy_inst 1 \
-partial_instrument 0 -race_verify_ml 0 \
-static_profile static_profile/static_profile_unittest.out \
-instrumented_lines static_profile/instrumented_lines_unittest.out \
-loop_range_lines static_profile/loop_range_lines_unittest.out \
-exiting_cond_lines static_profile/exiting_cond_lines_unittest.out \
-cond_wait_lines static_profile/cond_wait_lines_unittest.out \
-enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 -debug_pthread 1 -debug_malloc 1 -- \
test/verify/unittest