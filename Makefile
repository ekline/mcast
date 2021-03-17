## LICENSE_BEGIN
#
#   Apache 2.0 License
#
#   SPDX:Apache-2.0
#
#   https://spdx.org/licenses/Apache-2.0
#
#   See LICENSE file in the top level directory.
#
## LICENSE_END

CXX := clang++
CXX_FLAGS := --std=c++17 -Werror -Wall

PROG := mcast

$(PROG): main.o
	$(CXX) $(CXX_FLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f *.o $(PROG)

%.o: %.cc
	$(CXX) $(CXX_FLAGS) -c -o $@ $<
