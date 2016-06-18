#!/bin/sh

# pro=bodytrack
# ~/parsec-2.1/bin/parsecmgmt -a run -p $pro -c gcc-pthreads -i simmedium -n 4 -s "\

tool_path=/home/yiranyaoqiu/RaceChecker

# pure dynamic data race detection
# /home/yiranyaoqiu/pin/pin -t $tool_path/build-debug/race_profiler.so \
# -enable_multilock_hb 1 -parallel_detector_number 0 -track_racy_inst 1 \
# -ignore_lib 1 -enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 \
# -debug_pthread 1 -debug_malloc 1 -- test/verify/verifier19

# pure dynamic data race verification
# when do the multigroup verification, set history_race_analysis 0
/home/yiranyaoqiu/pin/pin -t $tool_path/build-debug/race_profiler.so \
-partial_instrument 1 -race_verify_ml 1 -history_race_analysis 1 \
-parallel_race_verify_ml 0 -parallel_verifier_number 0 \
-harmful_race_analysis 1 \
-static_profile $tool_path/static_profile/static_profile_19.out \
-instrumented_lines $tool_path/static_profile/instrumented_lines_19.out \
-ignore_lib 1 -enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 \
-debug_pthread 1 -debug_malloc 1 -- test/verify/verifier19

# spinning read loop and cond_wait read loop analysis
# /home/yiranyaoqiu/pin/pin -t $tool_path/build-debug/race_profiler.so \
# -enable_multilock_hb 1 -parallel_detector_number 2 -track_racy_inst 1 \
# -loop_range_lines $tool_path/static_profile/loop_range_lines_fft.out \
# -exiting_cond_lines $tool_path/static_profile/exiting_cond_lines_fft.out \
# -cond_wait_lines $tool_path/static_profile/cond_wait_lines_fft.out \
# -ignore_lib 1 -enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 \
# -debug_pthread 1 -debug_malloc 1 -- ~/splash2_origin/codes/kernels/fft/FFT -p2 -m16 -n65536 -l4

# pre grouping analysis
# /home/yiranyaoqiu/pin/pin -t $tool_path/build-debug/race_profiler.so \
# -enable_pre_group 1 -sorted_static_profile $tool_path/static_profile/sorted_static_profile_water.out \
# -partial_instrument 1 -static_profile $tool_path/static_profile/static_profile_water.out \
# -instrumented_lines $tool_path/static_profile/instrumented_lines_water.out \
# -ignore_lib 1 -enable_debug 0 -debug_pthread 1 -debug_mem 1 -debug_main 1 \
# -debug_pthread 1 -debug_malloc 1 -- ~/splash2_origin/codes/apps/water-nsquared/WATER-NSQUARED < ~/splash2_origin/codes/apps/water-nsquared/input