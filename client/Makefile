include ../common.inc

PROJECT_TARGET := stunclient
PROJECT_OBJS := clientmain.o
	
PROJECT_INTERMEDIATES := usage.txtcode usagelite.txtcode	

INCLUDES := $(BOOST_INCLUDE) -I../common -I../stuncore -I../networkutils
LIB_PATH := -L../common -L../stuncore -L../networkutils
LIBS := -lnetworkutils -lstuncore -lcommon -lcrypto


all: $(PROJECT_TARGET)

clean:
	rm -f $(PROJECT_OBJS) $(PROJECT_TARGET) $(PROJECT_INTERMEDIATES)

$(PROJECT_TARGET): $(PROJECT_OBJS)
	$(LINK.cpp) -o $@ $^ $(LIB_PATH) $(LIBS)


clientmain.cpp: usage.txtcode usagelite.txtcode


%.txtcode: %.txt
	sh ../server/makecodefile.sh $< $@ $(*)_text
