all: binding.so
	$(QJS) --napi --std -I ../polyfill.js test.js

binding.so: binding.cc myobject.cc
	$(CXX) -I.. -Wall -Wextra -Wno-unused-parameter -shared -fPIC -o $@ $^
