include ../common.inc

PROJECT_TARGET := stunclient
PROJECT_OBJS := clientmain.o
	

INCLUDES := $(BOOST_INCLUDE) -I../common -I../stuncore -I../networkutils -I../resources
LIB_PATH := -L../common -L../stuncore -L../networkutils
LIBS := -lnetworkutils -lstuncore -lcommon 


all: $(PROJECT_TARGET)

clean:
	rm -f $(PROJECT_OBJS) $(PROJECT_TARGET) 

$(PROJECT_TARGET): $(PROJECT_OBJS)
	$(LINK.cpp) -o $@ $^ $(LIB_PATH) $(LIBS) $(SOCKET_LIBS) $(CRYPTO_LIBS) $(ASLR_FLAGS) $(PGO_LINK_FLAGS)



