#!/bin/sh

# pro=bodytrack
# ~/parsec-2.1/bin/parsecmgmt -a run -p $pro -c gcc-pthreads -i simmedium -n 4 -s "\
/home/yiranyaoqiu/pin/pin -t /home/yiranyaoqiu/RaceChecker/build-debug/race_profiler.so \
-ignore_lib 1  -enable_pre_group 1 -track_racy_inst 1 \
-partial_instrument 1 -race_verify_ml 0 \
-sorted_static_profile static_profile/sorted_static_profile_10.out \
-static_profile static_profile/static_profile_10.out \
-instrumented_lines static_profile/instrumented_lines_10.out \
-loop_range_lines static_profile/loop_range_lines_10.out \
-exiting_cond_lines static_profile/exiting_cond_lines_10.out \
-cond_wait_lines static_profile/cond_wait_lines_10.out \
-enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 -debug_pthread 1 -debug_malloc 1 -- \
test/verify/verifier10