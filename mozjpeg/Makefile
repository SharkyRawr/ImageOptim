build: all

all: src/jpeglib.h

src/jpeglib.h:
	../scripts/fetch-submodule src
	test -f $@

.PHONY: build all install clean
