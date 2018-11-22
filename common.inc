#BOOST_INCLUDE := -I/Users/jselbie/boost_1_63_0
#OPENSSL_INCLUDE := -I/Users/jselbie/openssl/include

DEFINES := -DNDEBUG

# CLANG compiler works fine
# CXX := /usr/bin/clang++

# C++ 11 compiler is required
# I'd prefer not to have to set this mode explicitly, but...
#    g++ version 6 and up will default to at least C++ 14 or higher, and no need to set there
# However...
#    g++ versions less than 6  support it, but require -std=c++11 or -gnu++11 passed
#    With -std=c++11 passed, Cygwin will hide APIs such as getnetworkinfo, so gnu++11 is needed (or other macro)
#    Even with XCode 10, Mac Clang compiler defaults to C++98
#    So, short of writing a maybe-portable script that sniffs compiler support...
#    Explicitly setting gnu++11 is the best (works on Clang too)
#    Maybe it's time to learn CMAKE or Autoconf
CPPSTD := -std=gnu++11

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


