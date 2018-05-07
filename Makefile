CXX=g++
CXXFLAGS=--std=c++11 -W -Wall -O3

SRCS=main.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

C4Master:$(OBJS)
	$(CXX) $(LDFLAGS) -o C4Master $(OBJS) $(LOADLIBES) $(LDLIBS)

.depend: $(SRCS)
	$(CXX) $(CXXFLAGS) -MM $^ > ./.depend

include .depend

clean:
	rm -f *.o .depend C4Master
