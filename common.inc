#BOOST_INCLUDE := -I/Users/jselbie/boost_1_63_0
#OPENSSL_INCLUDE := -I/Users/jselbie/openssl/include

DEFINES := -DNDEBUG

# CLANG compiler works fine
# CXX := /usr/bin/clang++

# g++ versions less than 6 will default to C++ 98
ifeq ($(CXX),g++)
  lessthansix := $(shell expr `g++ -dumpversion | cut -d '.' -f 1` \< 6 )
  ifeq ($(lessthansix), 1)
    CPPSTD := -std=gnu++11
  endif
endif

STANDARD_FLAGS := -Wall -Wuninitialized 

RELEASE_FLAGS := -O2
DEBUG_FLAGS := -g
PROFILE_FLAGS := -O2 -g
FLAVOR_FLAGS = $(RELEASE_FLAGS)
CRYPTO_LIBS := -lcrypto


#SOLARIS HACK
UNAME := $(shell uname -s)
ifeq ($(UNAME),SunOS)
  SOCKET_LIBS := -lsocket -lnsl
endif

#Mac hack
ifeq ($(UNAME),Darwin)
  CRYPTO_LIBS :=
endif

.PHONY: all clean debug

%.hpp.gch: %.hpp
	echo Building precompiled header: $@
	$(COMPILE.cpp) $(CPPSTD) $(INCLUDES) $(DEFINES) $(STANDARD_FLAGS) $(FLAVOR_FLAGS) $^

%.o: %.cpp
	$(COMPILE.cpp) $(CPPSTD) $(INCLUDES) $(DEFINES) $(STANDARD_FLAGS) $(FLAVOR_FLAGS) $^

# put "all" target first so that it is the default
all:

debug: FLAVOR_FLAGS = $(DEBUG_FLAGS)
debug: DEFINES = -DDEBUG
debug: all


profile: FLAVOR_FLAGS = $(PROFILE_FLAGS)
profile: all


