CXXFLAGS := -Werror -g -fno-omit-frame-pointer -pthread

targets := $(basename $(wildcard *.cc))

objs := $(targets:%=%.o)

.PHONY:all clean

all:$(targets)

$(targets):%:%.o
	$(CXX)  $(CXXFLAGS) -o $@ $<

$(objs):%.o:%.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(targets)