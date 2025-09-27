all: binding.so
	$(QJS) --napi --std -I ../polyfill.js test.js

binding.so: test_string.c
	$(CXX) -I.. -Wall -Wextra -Wno-unused-parameter -shared -fPIC -o $@ $^
