all: binding.so
	$(QJS) --std -I ../polyfill.js test.js

binding.so: test_symbol.c
	$(CXX) -I.. -Wall -Wextra -Wno-unused-parameter -shared -fPIC -o $@ $^
