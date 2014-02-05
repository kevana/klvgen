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

clean:
	rm -rf klvgen *.dSYM
