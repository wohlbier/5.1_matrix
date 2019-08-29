SRCS = main.cc
EMU_OBJS = $(subst .cc,.emu.o,$(SRCS))

#EMU_PATH = /local/devel/packages/emu-18.11-cplus
#EMU_PATH = /local/devel/packages/emu-19.02
EMU_PATH = /home/jgwohlbier/devel/packages/emu-19.02
EMU_CXX = $(EMU_PATH)/bin/emu-cc
EMU_SIM = $(EMU_PATH)/bin/emusim.x

EMU_SIM_ARGS =
#EMU_SIM_ARGS += --model_4nodelet_hw
#EMU_SIM_ARGS += --chick_box
#EMU_SIM_ARGS += -m 34

#EMU_SIM_ARGS += --verbose_isa
#EMU_SIM_ARGS += --verbose_tid
#EMU_SIM_ARGS += --short_trace
#EMU_SIM_ARGS += --memory_trace
EMU_SIM_ARGS += --capture_timing_queues

EMU_PROFILE = $(EMU_PATH)/bin/emusim_profile

LDFLAGS = -lemu_c_utils

EXE  = matrix
EMU_EXE = $(EXE).mwx

$(EMU_EXE) : $(EMU_OBJS)
	$(EMU_CXX) -o $(EMU_EXE) $(EMU_OBJS) $(LDFLAGS)

run : $(EMU_EXE)
	$(EMU_SIM) $(EMU_SIM_ARGS) $(EMU_EXE)

profile : $(EMU_EXE)
	CORE_CLK_MHZ=175 \
	$(EMU_PROFILE) profile $(EMU_SIM_ARGS) -- $(EMU_EXE)

%.emu.o: %.cc
	$(EMU_CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY : clean

clean :
	-$(RM) *~ $(OBJS) $(EMU_OBJS) $(EXE) $(EMU_EXE) *.cdc *.hdd *.vsf
	-$(RM) -r profile
