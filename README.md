#klvgen

This is a proof-of-concept tool that generates a MISB 601.2 compliant metadata packet stream for interpolation with video streams. It includes a running timestamp and several static data fields.

##Typical use case 
We want information such as the timestamp and gps coordinates of a drone's camera feed, but we don't want to worry about timestamps getting out of sync with the video or matching  metadata to the right video stream.

##Compilation
The makefile includes targets for linux, OS X, and Windows. The default target is linux:

    $ make            (linux)
    $ make osx        (OS X)
    $ make win32      (Windows)

If you are compiling for windows and do not have make installed, use the following:

    $ gcc -Wall -o klvgen.exe klvgen.c -D WIN32 -lwsock32

Note: This has been only been tested on windows XP and 7 using MinGW.

##Usage
If run with no arguments, klvgen will start with default values for connection parameters and fields.

    $ ./klvgen

For a full listing of available parameters, run klvgen with the `-h` or `--help` flags.

    $ ./klvgen -h
    $ ./klvgen --help
