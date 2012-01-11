include ../common.inc

PROJECT_TARGET := stunserver
PROJECT_OBJS := main.o server.o stunconnection.o stunsocketthread.o tcpserver.o

PROJECT_INTERMEDIATES := usage.txtcode usagelite.txtcode


INCLUDES := $(BOOST_INCLUDE) -I../common -I../stuncore -I../networkutils
LIB_PATH := -L../common -L../stuncore -L../networkutils
LIBS := -lnetworkutils -lstuncore -lcommon -pthread -lcrypto


all: $(PROJECT_TARGET)

clean:
	rm -f $(PROJECT_OBJS) $(PROJECT_TARGET) $(PROJECT_INTERMEDIATES)

$(PROJECT_TARGET): $(PROJECT_OBJS)
	$(LINK.cpp) -o $@ $^ $(LIB_PATH) $(LIBS)

main.cpp: usage.txtcode usagelite.txtcode


%.txtcode: %.txt
	sh makecodefile.sh $< $@ $(*)_text



