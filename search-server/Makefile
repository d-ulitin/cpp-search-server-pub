CPP := g++
CPPFLAGS := -c -std=c++17 -Wall -Wextra -Wpedantic -O2 -g
LD := $(CPP)
CPPSOURCE := $(wildcard *.cpp)
CPPHEADERS := $(wildcard *.h)
OBJECTS := $(CPPSOURCE:.cpp=.o)
DEPS := $(CPPSOURCE:.cpp=.dep)
# GCH := $(CPPHEADERS:.h=.h.gch)
TARGET := search-server.out

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $(OBJECTS) -o $(TARGET) -ltbb -lpthread

%.o: %.cpp
	$(CPP) $(CPPFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(TARGET)

MAKEFILEDEPS := Makefile.deps

include $(MAKEFILEDEPS)

$(MAKEFILEDEPS):
	$(CPP) $(CPPFLAGS) -MM $(CPPSOURCE) > Makefile.deps

.PHONY: cleandeps
cleandeps:
	rm -f $(MAKEFILEDEPS)

.PHONY: cleanall
cleanall: clean cleandeps

# deps: $(DEPS)

# %.dep: %.cpp
# 	$(CPP) -MM $< > $@

# include $(DEPS)

# .PHONY: cleandeps
# cleandeps:
# 	rm -f $(DEPS)

# headers: $(GCH)

# %.h.gch: %.h
# 	$(CPP) $(CPPFLAGS) $< -o $@

