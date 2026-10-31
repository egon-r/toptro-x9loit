IMG ?= xt36694-build
CROSS ?= arm-linux-gnueabihf-
BINS := jail xtinfo xt36694
NATIVE := $(shell command -v $(CROSS)gcc 2>/dev/null)

all: $(BINS:%=bin/%)
ifndef NATIVE
all: image
endif

image:
	docker build -t $(IMG) -f ../harness/Dockerfile.build ../harness
bin/%: %.c ipt_blob.h | bin image
ifdef NATIVE
	$(CROSS)gcc -static -Os -g -Wall -Wextra -pthread -o $@ $<
else
	docker run --rm -v "$(CURDIR)":/w -w /w $(IMG) \
	  $(CROSS)gcc -static -Os -g -Wall -Wextra -pthread -o $@ $<
endif

bin:
	mkdir -p bin

clean:
	rm -rf bin

.PHONY: all image clean
