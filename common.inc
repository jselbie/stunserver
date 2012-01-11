BOOST_INCLUDE := -I/home/jselbie/boost_1_48_0
# OPENSSL_INCLUDE := -I/home/jselbie/lib/openssl

DEFINES := -DNDEBUG

STANDARD_FLAGS := -Wall -Wuninitialized 

RELEASE_FLAGS := -O2
DEBUG_FLAGS := -g
FLAVOR_FLAGS = $(RELEASE_FLAGS)

.PHONY: all clean debug

%.h.gch: %.h
	echo Building precompiled header: $@
	$(COMPILE.cpp) $(INCLUDES) $(DEFINES) $(STANDARD_FLAGS) $(FLAVOR_FLAGS) $^

%.o: %.cpp
	$(COMPILE.cpp) $(INCLUDES) $(DEFINES) $(STANDARD_FLAGS) $(FLAVOR_FLAGS) $^

# put "all" target first so that it is the default
all:

debug: FLAVOR_FLAGS = $(DEBUG_FLAGS)
debug: DEFINES = -DDEBUG
debug: all




