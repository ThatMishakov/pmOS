HEADER_SRC = include
HEADER_DEST = usr/include

# List of headers to copy (using wildcards)
HEADERS_SRC := $(shell find include/ -type f -name '*.h') $(shell find include/ -type f -name '*.hh')
HEADERS_DEST := $(patsubst $(HEADER_SRC)/%, $(HEADER_DEST)/%, $(HEADERS_SRC))

# Default target
all: $(HEADERS_DEST)

# Target to copy each header to the sysroot directory
$(HEADER_DEST)/%.h: $(HEADER_SRC)/%.h
	@mkdir -p $(dir $@)
	cp $< $@

$(HEADER_DEST)/%.hh: $(HEADER_SRC)/%.hh
	@mkdir -p $(dir $@)
	cp $< $@

# Optional: Clean up the headers from the sysroot directory
clean:
	@rm -rf $(HEADER_DEST)/*.h
