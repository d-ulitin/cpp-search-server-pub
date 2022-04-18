CPP := g++
CPPFLAGS := -c -std=c++17 -Wall -Wextra -Wpedantic -O2 -g
LD := $(CPP)
CPPSOURCE := $(wildcard *.cpp)
CPPHEADERS := $(wildcard *.h)
OBJECTS := $(CPPSOURCE:.cpp=.o)
DEPS := $(CPPSOURCE:.cpp=.dep)
TARGET := search-server.out

all: deps $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $(OBJECTS) -o $(TARGET) -ltbb -lpthread

%.o: %.cpp
	$(CPP) $(CPPFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: cleanall
cleanall: clean cleandeps

.PHONY: deps
deps: $(DEPS)

%.dep: %.cpp
#	$(CPP) -MM $< > $@
# 	replace 'main.o: ...' to 'main.o main.dep: ...'
	$(CPP) -MM $< | sed -r 's/^(.*)[.]o:/\1.o \1.dep:/' > $@ 

include $(DEPS)

.PHONY: cleandeps
cleandeps:
	rm -f $(DEPS)

# GCH := $(CPPHEADERS:.h=.h.gch)

# headers: $(GCH)

# %.h.gch: %.h
# 	$(CPP) -x c++-header -std=c++17 $< -o $@

