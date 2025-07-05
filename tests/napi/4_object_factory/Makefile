all: binding.so
	$(QJS) --napi --napi --std -I ../polyfill.js test.js

binding.so: binding.c
	$(CXX) -I.. -Wall -Wextra -Wno-unused-parameter -shared -fPIC -o $@ $^
