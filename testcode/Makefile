include ../common.inc

PROJECT_TARGET := stuntestcode
PROJECT_OBJS := benchmark.o testbuilder.o testclientlogic.o testcmdline.o testcrc32.o testcode.o testdatastream.o testfasthash.o testintegrity.o testmessagehandler.o testpolling.o testratelimiter.o testreader.o testrecvfromex.o 
 
INCLUDES := $(BOOST_INCLUDE) $(OPENSSL_INCLUDE) -I../common -I../stuncore -I../networkutils
LIB_PATH := -L../networkutils -L../stuncore -L../common
LIBS := -lnetworkutils -lstuncore -lcommon -lpthread 


all: $(PROJECT_TARGET)

clean:
	rm -f $(PROJECT_OBJS) $(PROJECT_TARGET)

$(PROJECT_TARGET): $(PROJECT_OBJS)
	$(LINK.cpp) -o $@ $^ $(LIB_PATH) $(LIBS) $(SOCKET_LIBS) $(CRYPTO_LIBS)



