
# Usage:
#   make install 
#   make build
#   make run
#   make clean
#   make distclean

SHELL := /bin/sh

PYTHON     ?= python3
VENV_DIR   ?= .venv
BUILD_DIR  ?= build
BUILD_TYPE ?= Debug

BIN_NAME   ?= codegencli
BIN        := $(BUILD_DIR)/$(BIN_NAME)

SCRIPT     ?= python/llm_adaptor.py
PROMPT     ?= etc/prompt.txt
OUT        ?= etc/gen.txt
RG         ?= rg

# python stuff
PY  := $(VENV_DIR)/bin/python
PIP := $(VENV_DIR)/bin/pip

PY_DEPS_STAMP := $(VENV_DIR)/.deps_installed

.PHONY: help install pydeps auth configure build run clean distclean check-rg submodules

help:
	@echo "Targets:"
	@echo "  install       - install deps"
	@echo "  build       - config cmake and build "
	@echo "  run         - run $(BIN_NAME) using venv python + script + prompt"
	@echo "  clean       - remove build dir"
	@echo "  distclean   - remove entire distribution"


check-rg:
	@RG_PATH="$$(command -v $(RG) 2>/dev/null)" ; \
	if [ -n "$$RG_PATH" ]; then \
		echo "OK: ripgrep found: $$RG_PATH" ; \
	else \
		echo "ERROR: ripgrep (rg) not found." ; \
		echo "Install:" ; \
		echo "  macOS:  brew install ripgrep" ; \
		echo "  Ubuntu/Debian: sudo apt-get install ripgrep" ; \
		echo "  Fedora: sudo dnf install ripgrep" ; \
		echo "  Arch: sudo pacman -S ripgrep" ; \
		exit 1 ; \
	fi


$(VENV_DIR):
	$(PYTHON) -m venv $(VENV_DIR)


$(PY_DEPS_STAMP): python/requirements.txt | $(VENV_DIR)
	@echo "Installing Python deps into $(VENV_DIR)..."
	@$(PY) -m pip install --upgrade pip >/dev/null
	@$(PIP) install -r python/requirements.txt
	@touch $(PY_DEPS_STAMP)
	@echo "OK: Python deps installed/updated"

install: check-rg pydeps submodules

submodules:
	git submodule update --init --recursive

pydeps: $(PY_DEPS_STAMP)
	@echo "OK: Python deps ready (stamp: $(PY_DEPS_STAMP))"

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR)


run: setup build 
	$(BIN) ask --py $(PY) --script $(SCRIPT) --prompt $(PROMPT)

clean:
	rm -rf $(BUILD_DIR)
	rm etc/context.txt
	rm etc/answer.txt
	rm etc/gen.txt

distclean: clean
	rm -rf $(VENV_DIR)
