CPP := g++
CPPFLAGS := -c -std=c++17 -Wall -Wextra -Wpedantic -O2 -g
LD := $(CPP)
CPPSOURCE := $(wildcard *.cpp)
CPPHEADERS := $(wildcard *.h)
OBJECTS := $(CPPSOURCE:.cpp=.o)
DEPS := $(CPPSOURCE:.cpp=.dep)
# GCH := $(CPPHEADERS:.h=.h.gch)
TARGET := search-server.out

$(TARGET): $(OBJECTS)
	$(LD) $(OBJECTS) -o $(TARGET) -ltbb -lpthread

deps: $(DEPS)

%.dep: %.cpp
	$(CPP) -MM $< > $@

include $(DEPS)

# headers: $(GCH)

# %.h.gch: %.h
# 	$(CPP) $(CPPFLAGS) $< -o $@

%.o: %.cpp
	$(CPP) $(CPPFLAGS) $< -o $@

.PHONY: clean

clean:
	rm $(OBJECTS)
	rm $(TARGET)

.PHONY: cleandeps

cleandeps:
	rm -f $(DEPS)
