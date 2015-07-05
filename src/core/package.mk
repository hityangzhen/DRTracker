# Rules for core package

protodefs += \
	core/static_info.proto

srcs += \
	core/debug_analyzer.cc \
	core/descriptor.cc \
	core/callstack.cc \
	core/knob.cc \
	core/execution_control.cpp \
	core/filter.cc \
	core/lock_set.cc \
	core/log.cc \
	core/pin_knob.cpp \
	core/pin_util.cpp \
	core/static_info.cc \
	core/static_info.pb.cc \
	core/vector_clock.cc \
	core/segment_set.cc \
	core/wrapper.cpp 

core_objs := \
	core/debug_analyzer.o \
	core/descriptor.o \
	core/callstack.o \
	core/knob.o \
	core/execution_control.o \
	core/filter.o \
  	core/lock_set.o \
  	core/log.o \
  	core/pin_knob.o \
  	core/pin_util.o \
  	core/static_info.o \
  	core/static_info.pb.o \
  	core/vector_clock.o \
  	core/segment_set.o \
  	core/wrapper.o

