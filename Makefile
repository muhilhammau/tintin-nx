# The Adventures of Tintin: The Game HD -- Nintendo Switch wrapper (AArch32)
TARGET               := tintin_nx
PORT_NPDM_PROGRAM_ID := 0x0100000000001099
PORT_NPDM_ADDRSPACE  := 2
include runtime/runtime.mk

.PHONY: imports
imports:
	python3 runtime/tools/gen_imports.py --libs .apk_libs
