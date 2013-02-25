# BOOST_INCLUDE := -I/home/jselbie/boost_1_51_0
# OPENSSL_INCLUDE := -I/home/jselbie/lib/openssl

DEFINES := -DNDEBUG

# CLANG compiler works fine
# CXX := /usr/bin/clang++

STANDARD_FLAGS := -Wall -Wuninitialized 

RELEASE_FLAGS := -O2
DEBUG_FLAGS := -g
PROFILE_FLAGS := -O2 -g
FLAVOR_FLAGS = $(RELEASE_FLAGS)


#SOLARIS HACK
UNAME := $(shell uname -s)
ifeq ($(UNAME),SunOS)
  SOCKET_LIBS := -lsocket -lnsl
endif




.PHONY: all clean debug

%.hpp.gch: %.hpp
	echo Building precompiled header: $@
	$(COMPILE.cpp) $(INCLUDES) $(DEFINES) $(STANDARD_FLAGS) $(FLAVOR_FLAGS) $^

%.o: %.cpp
	$(COMPILE.cpp) $(INCLUDES) $(DEFINES) $(STANDARD_FLAGS) $(FLAVOR_FLAGS) $^

# put "all" target first so that it is the default
all:

debug: FLAVOR_FLAGS = $(DEBUG_FLAGS)
debug: DEFINES = -DDEBUG
debug: all


profile: FLAVOR_FLAGS = $(PROFILE_FLAGS)
profile: all


