# Top level makefile for the project

compilertype ?= debug
packages := core race

ifeq ($(compilertype),debug)
	debug := 1
endif

# include Protobuf environment
include protobuf.mk

# include PIN environment
include pin.mk

# define source,build dir
srcdir := src/

ifeq ($(compilertype),debug)
	builddir := build-debug/
else ifeq ($(compilertype),release)
	builddir := build-release/
else
	$(error Please specify compile type correctly,debug or release.)
endif

# process rules for each package
$(foreach pkg,$(packages),$(eval include $(srcdir)$(pkg)/package.mk))
$(foreach pkg,$(packages),$(eval objdirs += $(builddir)$(pkg)/))
protosrcs := $(protodefs:%.proto=$(srcdir)%.pb.cc)
protohdrs := $(protodefs:%.proto=$(srcdir)%.pb.h)

cxxsrcs := $(filter %.cc,$(srcs:%=$(srcdir)%))
pincxxsrcs := $(filter %.cpp,$(srcs:%=$(srcdir)%))
cxxobjs := $(cxxsrcs:$(srcdir)%.cc=$(builddir)%.o)
pincxxobjs := $(pincxxsrcs:$(srcdir)%.cpp=$(builddir)%.o)
pintool_names := $(basename $(pintools))

pintools := $(pintools:%=$(builddir)%)

$(foreach name,$(pintool_names),$(eval $(name)_objs := $($(name)_objs:%=$(builddir)%)))

# set compile flags
CFLAGS += -fPIC
CXXFLAGS += -fPIC
INCS += -I$(srcdir)
LIBS += -lprotobuf

TOOL_LIBS += -lprotobuf

ifeq ($(compilertype),debug)
	CFLAGS += -Wall -Werror -g -DDEBUG
	CXXFLAGS += -Wall -Werror -g -DDEBUG
endif

# gen dependency
cxxgendepend = $(CXX) $(CXXFLAGS) $(INCS) -MM -MT $@ -MF $(builddir)$*.d $<
pincxxgendepend = $(CXX) $(CXXFLAGS) $(TOOL_INCLUDES) $(TOOL_CXXFLAGS) $(INCS) -MM -MT $@ -MF $(builddir)$*.d $<


# set the default goal to be all
.DEFAULT_GOAL := race-checker

# rules
.SECONDEXPANSION:

race-checker: $(pintools)

$(cxxobjs) : | $(objdirs)

$(pincxxobjs) : | $(objdirs)

$(cxxobjs) : $(protosrcs) $(protohdrs)

$(pincxxobjs) : $(protosrcs) $(protohdrs)


$(objdirs):
	mkdir -p $@

$(protosrcs): $(srcdir)%.pb.cc : $(srcdir)%.proto
	$(PROTOC) -I=$(srcdir) --cpp_out=$(srcdir) $<

$(protohdrs): $(srcdir)%.pb.h : $(srcdir)%.proto
	$(PROTOC) -I=$(srcdir) --cpp_out=$(srcdir) $<


$(cxxobjs): $(builddir)%.o : $(srcdir)%.cc
	@$(cxxgendepend);
	$(CXX) -c $(CXXFLAGS) $(INCS) -o $@ $<

$(pincxxobjs): $(builddir)%.o : $(srcdir)%.cpp
	@$(pincxxgendepend);
	$(CXX) $(CXXFLAGS) $(TOOL_INCLUDES) $(TOOL_CXXFLAGS) $(INCS) $(COMP_OBJ)$@ $<

$(pintools): $(builddir)%.so : $$(%_objs)
	$(LINKER) $(TOOL_LDFLAGS) $(LINK_DEBUG) ${LINK_EXE}$@ $^ ${TOOL_LPATHS} $(TOOL_LIBS) $(DBG)

clean:
	rm -rf build-debug build-release
	rm -f $(protosrcs) $(protohdrs)

# include dependencies
-include $(cxxsrcs:$(srcdir)%.cc=$(builddir)%.d)
-include $(pincxxsrcs:$(srcdir)%.cpp=$(builddir)%.d)




