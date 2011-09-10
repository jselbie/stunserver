include ../common.inc

PROJECT_TARGET := libnetworkutils.a
PROJECT_OBJS := adapters.o recvfromex.o resolvehostname.o stunsocket.o
INCLUDES := $(BOOST_INCLUDE) -I../common -I../stuncore


.PHONY: all
.PHONY: clean


all: $(PROJECT_TARGET)

clean:
	rm -f $(PROJECT_OBJS) $(PROJECT_TARGET)

$(PROJECT_TARGET): $(PROJECT_OBJS)
	rm -f $@
	$(AR) rv $@ $^ 




