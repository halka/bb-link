ROOT := $(abspath ..)
BUILD_DIR := $(CURDIR)/build
NATIVE_TEST := $(BUILD_DIR)/KISSInterceptorTest

.PHONY: tests runtests epoxy-tests epoxy-runtests epoxy-clean clean

tests: $(NATIVE_TEST)

$(NATIVE_TEST): native/KISSInterceptorTest.cpp $(ROOT)/src/bb-link/KISSInterceptor.cpp $(ROOT)/src/bb-link/KISSInterceptor.h
	mkdir -p $(BUILD_DIR)
	$(CXX) -std=c++11 -Wall -Wextra -Werror -I$(ROOT)/src/bb-link \
		native/KISSInterceptorTest.cpp $(ROOT)/src/bb-link/KISSInterceptor.cpp -o $@

epoxy-tests:
	set -e; \
	for i in *Test/Makefile; do \
		echo '==== Making:' $$(dirname $$i); \
		$(MAKE) -C $$(dirname $$i); \
	done

runtests: tests
	$(NATIVE_TEST)

epoxy-runtests:
	set -e; \
	for i in *Test/Makefile; do \
		echo '==== Running:' $$(dirname $$i); \
		$(MAKE) -C $$(dirname $$i) run; \
	done

clean:
	$(RM) -r $(BUILD_DIR)

epoxy-clean:
	set -e; \
	for i in *Test/Makefile; do \
		echo '==== Cleaning:' $$(dirname $$i); \
		$(MAKE) -C $$(dirname $$i) clean; \
	done
