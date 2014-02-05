# ============================================================================
# Makefile for klvgen
#
# Author: Kevan Ahlquist
# All rights reserved
# ============================================================================

linux:
	cc -Wall -g -o klvgen main.c -lrt

osx:
	cc -Wall -g -o klvgen main.c

win32:
	cc -Wall -o klvgen.exe klvgen.c -D WIN32 -lwsock32

clean:
	rm -rf klvgen *.dSYM
