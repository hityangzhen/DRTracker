# Rules for the race package

protodefs += \
	race/race.proto

srcs += \
  race/detector.cc \
  race/djit.cc \
  race/eraser.cc \
  race/race_track.cc \
  race/helgrind.cc \
  race/thread_sanitizer.cc \
  race/fast_track.cc \
  race/literace.cc \
  race/loft.cc \
  race/acculock.cc \
  race/multilock_hb.cc \
  race/simple_lock.cc \
  race/simplelock_plus.cc \
  race/verifier.cpp \
  race/verifier_sl.cpp \
  race/adhoc_sync.cc \
  race/verifier_ml.cpp \
  race/profiler.cpp \
  race/profiler_main.cpp \
  race/race.cc \
  race/race.pb.cc

pintools += \
	race_profiler.so

race_profiler_objs := \
  race/verifier.o \
  race/verifier_ml.o \
  race/profiler.o \
  race/profiler_main.o \
  race/race.o \
  race/race.pb.o \
  $(core_objs)

race_objs := \
  race/detector.o \
  race/djit.o \
  race/eraser.o \
  race/race_track.o \
  race/helgrind.o \
  race/thread_sanitizer.o \
  race/fast_track.o \
  race/literace.o \
  race/acculock.o \
  race/multilock_hb.o \
  race/loft.o \
  race/simple_lock.o \
  race/simplelock_plus.o \
  race/verifier.o \
  race/verifier_sl.o \
  race/adhoc_sync.o \
  race/verifier_ml.o \
  race/race.o \
  race/race.pb.o
  
