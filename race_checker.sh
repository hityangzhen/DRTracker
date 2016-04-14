#!/bin/sh

# pro=bodytrack
# ~/parsec-2.1/bin/parsecmgmt -a run -p $pro -c gcc-pthreads -i simmedium -n 4 -s "\

tool_path=/home/yiranyaoqiu/RaceChecker

# pure dynamic data race detection
# /home/yiranyaoqiu/pin/pin -t $tool_path/build-debug/race_profiler.so \
# -enable_multilock_hb 1 -parallel_detector_number 1 -track_racy_inst 1 \
# -ignore_lib 1 -enable_debug 1 -debug_pthread 1 -debug_mem 1 -debug_main 1 \
# -debug_pthread 1 -debug_malloc 1 -- test/test16

# pure dynamic data race verification
# when do the multigroup verification, set history_race_analysis 0
/home/yiranyaoqiu/pin/pin -t $tool_path/build-debug/race_profiler.so \
-partial_instrument 1 -race_verify_ml 1 -history_race_analysis 0 \
-parallel_race_verify_ml 1 -parallel_verifier_number 1 \
-static_profile $tool_path/static_profile/static_profile_8.out \
-instrumented_lines $tool_path/static_profile/instrumented_lines_8.out \
-ignore_lib 1 -enable_debug 1 -debug_pthread 1 -debug_mem 1 -debug_main 1 \
-debug_pthread 1 -debug_malloc 1 -- test/verify/verifier8

# spinning read loop and cond_wait read loop analysis
# /home/yiranyaoqiu/pin/pin -t $tool_path/build-debug/race_profiler.so \
# -enable_multilock_hb 1 -track_racy_inst 1 \
# -loop_range_lines $tool_path/static_profile/loop_range_lines_unittest.out \
# -exiting_cond_lines $tool_path/static_profile/exiting_cond_lines_unittest.out \
# -cond_wait_lines $tool_path/static_profile/cond_wait_lines_unittest.out \
# -ignore_lib 1 -enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 \
# -debug_pthread 1 -debug_malloc 1 -- test/verify/unittest

# pre grouping analysis
# /home/yiranyaoqiu/pin/pin -t $tool_path/build-debug/race_profiler.so \
# -enable_pre_group 1 -sorted_static_profile $tool_path/static_profile/sorted_static_profile_bank_count.out \
# -partial_instrument 1 -static_profile $tool_path/static_profile/static_profile_bank_count.out \
# -instrumented_lines $tool_path/static_profile/instrumented_lines_bank_count.out \
# -ignore_lib 1 -enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 \
# -debug_pthread 1 -debug_malloc 1 -- example/bank_count/main