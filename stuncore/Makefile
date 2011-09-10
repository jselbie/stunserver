include ../common.inc

PROJECT_TARGET := libstuncore.a
PROJECT_OBJS := buffer.o datastream.o messagehandler.o socketaddress.o stunbuilder.o stunclientlogic.o stunclienttests.o stunreader.o stunutils.o
INCLUDES := $(BOOST_INCLUDE) $(OPENSSL_INCLUDE) -I../common

all: $(PROJECT_TARGET)

clean:
	rm -f $(PROJECT_OBJS) $(PROJECT_TARGET)

$(PROJECT_TARGET): $(PROJECT_OBJS)
	rm -f $@
	$(AR) rv $@ $^ 


