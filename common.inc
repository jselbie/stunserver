#BOOST_INCLUDE := -I/home/jselbie/boost_1_73_0
#OPENSSL_INCLUDE := -I/Users/jselbie/openssl/include

DEFINES := -DNDEBUG

# CLANG compiler works fine
# CXX := /usr/bin/clang++

STANDARD_FLAGS := -Wall -Wuninitialized 

RELEASE_FLAGS := -O3
DEBUG_FLAGS := -g
PROFILE_FLAGS := -O3 -g
FLAVOR_FLAGS = $(RELEASE_FLAGS)
CRYPTO_LIBS := -lcrypto
PGO1_FLAGS := -fprofile-generate -fprofile-dir=/home/jselbie/profile
PGO2_FLAGS := -fprofile-use -fprofile-dir=/home/jselbie/profile


#SOLARIS HACK
UNAME := $(shell uname -s)
UNAMEOS := $(shell uname -o)

ifeq ($(UNAME),SunOS)
  SOCKET_LIBS := -lsocket -lnsl
endif

#Mac hack
ifeq ($(UNAME),Darwin)
  CRYPTO_LIBS :=
endif

#Cygwin hack for ASLR
ifeq ($(UNAMEOS), Cygwin)
  ASLR_FLAGS := -Xlinker --dynamicbase
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


pgo1: FLAVOR_FLAGS = $(PGO1_FLAGS) $(RELEASE_FLAGS)
pgo1: PGO_LINK_FLAGS = $(PGO1_FLAGS)
pgo1: all


pgo2: FLAVOR_FLAGS = $(PGO2_FLAGS) $(RELEASE_FLAGS)
pgo2: PGO_LINK_FLAGS = $(PGO2_FLAGS)
pgo2: all


