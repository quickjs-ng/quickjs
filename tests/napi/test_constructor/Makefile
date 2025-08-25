all: binding.so
	$(QJS) --std -I ../polyfill.js test.js

binding.so: test_constructor.c test_constructor_name.c
	$(CXX) -I.. -Wall -Wextra -Wno-unused-parameter -shared -fPIC -o $@ $^
