all: binding.so
	$(QJS) --napi --std -I ../polyfill.js test.js

binding.so: compare_env.c store_env.c
	$(CXX) -I.. -Wall -Wextra -Wno-unused-parameter -shared -fPIC -o $@ $^
