//============================================================================
//		UDP packet generator
// Generates a UDP stream to MISB 601.2 specs including the following parameters:
// Key: LDS Universal key
// Timestamp: UNIX, in microseconds from midnight Jan. 1, 1970
// Mission ID: ASCII field, 12 character length
// Platform Designation: ASCII field, 12 character length
// Sensor Latitude: Degrees, -90 to +90
// Sensor Longitude: Degrees, -180 to +180
// Sensor True Altitude: Meters, -900 to +19000 meters
// UAS LDS Version: 0x02, code for MISB 601.2 spec
// Checksum: Generated for every packet
//
// Compilation for Windows:
//   gcc -Wall -o klvgen.exe klvout.c -D WIN32 -lwsock32
//
// Compilation for UNIX:
//   make (requires included Makefile)
// 		OR
//   gcc -Wall -g -o klvgen -lrt main.c
//
// Example usage: ./klvgen -a 127.0.0.1 -p 9000 -r 1 -m "Mission 01" -n "Demo" -t 45.2 -g -93 -e 200
//
// Author: Kevan Ahlquist
// All rights reserved
//============================================================================

#include "klvgen.c"

//============================================================================
int main(int argc, char *argv[]) {
#ifndef WIN32
	signal(SIGINT, exitProgram);
	signal(SIGTERM, exitProgram);
	signal(SIGHUP, exitProgram);
	signal(SIGKILL, exitProgram);
#endif

	timestamp = updateTimestamp();
	
	// Set Default values
	sendRate = 1.0;
	strcpy(missionId, "Mission 01");
	strcpy(platform, "Demo");
	latitude = (uint32_t)htonl(mapLatitude("44.64423"));
	longitude = (uint32_t)htonl(mapLongitude("-93.24013"));
	altitude = (uint16_t)htons(mapAltitude("333"));
	strcpy(address, "127.0.0.1");
	servPort = 9000;
	DEBUG = 0;
	
	printf("\nUDP Generator, Version 1.0.1\nKevan Ahlquist\nAll Rights Reserved\n\n");
	
	// read user options
	int option_index = 0;
	char optc;
	static struct option long_options[] =
		{
		 {"address", 		required_argument, 0, 'a'},
		 {"port", 	 		required_argument, 0, 'p'},
		 {"rate",  	 		required_argument, 0, 'r'},
		 {"mission-id", required_argument, 0, 'm'},
		 {"platform",   required_argument, 0, 'n'},
		 {"latitude",   required_argument, 0, 't'},
		 {"longitude",  required_argument, 0, 'g'},
		 {"altitude",   required_argument, 0, 'e'},
		 {"help", no_argument,       0, 'h'},
		 {"version", no_argument,       0, 'v'},
		 {0, 0, 0, 0}
		};
	while (( optc = getopt_long(argc, argv, "a:p:r:m:n:t:g:e:hv", long_options, &option_index)) != -1) {
		switch(optc) {
			case 'a':
				strncpy(address, optarg, 16);
				address[15] = '\0'; // Prevent buffer overrun
				printf("Address received: %s\n", address);
				break;
			case 'p':
				servPort = atol(optarg);
				printf("Port received: %d\n", servPort);
				break;
			case 'r':
				sendRate = atof(optarg);
				printf("Rate received: %f\n", sendRate);
				if (sendRate > 1000000) {
					printf("Values greater than 1,000,000 packets per second are not supported\n");
					exit(0);
				}
				break;
			case 'm':
				strncpy(missionId, optarg, 12);
				missionId[12] = '\0'; // Prevent buffer overrun
				if (strlen(optarg) > 12) printf("WARNING: Mission ID truncated to 12 characters\n");
				printf("Mission ID received: %s\n", missionId);
				break;
			case 'n':
				strncpy(platform, optarg, 12);
				platform[12] = '\0'; // Prevent buffer overrun
				if (strlen(optarg) > 12) printf("WARNING: Platform truncated to 12 characters\n");
				printf("Platform received: %s\n", platform);
				break;
			case 't':
				latitude = (uint32_t)htonl(mapLatitude(optarg));
				printf("Latitude received: %s\n", optarg);
				if (atof(optarg) < -90.0 || atof(optarg) > 90.0) {
					printf("ERROR: Latitude out of range (-90,90)\n");
					exit(0);
				}
				break;
			case 'g':
				longitude = (uint32_t)htonl(mapLongitude(optarg));
				printf("Longitude received: %s\n", optarg);
				if (atof(optarg) < -180.0 || atof(optarg) > 180.0) {
					printf("ERROR: Longitude out of range (-180,180)\n");
					exit(0);
				}
				break;
			case 'e':
				altitude = (uint16_t)htons(mapAltitude(optarg));
				printf("Altitude received: %s\n", optarg);
				if (atof(optarg) < -900 || atof(optarg) > 19000) {
					printf("ERROR: Altitude out of range(-900,19000)\n");
					exit(0);
				}
				break;
			case 'h':
				help();
				exit(0);
				break;
			case 'v':
				exit(0);
				break;
			default:
				printf("Usage: klvgen -a <address>:<port> -r <rate> -m<mission-id> -p <platform> -t <lat> -g <long> -e <elev>\n");
				printf("For help use option -h or --help\n");
				exit(0);
		}
	}
	
	if (udpInit() == -1) exit(-1);
	
	// TESTING ================================================================
	int i;
	if (DEBUG) {
		printf("Testing mapping functions===============\n");
		printf("map 0 from 0-10 to 0-100: %d\n", mapValue(0,0,10,0,100));
		printf("map 10 from 0-10 to 0-100: %d\n", mapValue(10,0,10,0,100));
		printf("map 5 from 0-10 to 0-100: %d\n", mapValue(5,0,10,0,100));
	}
	if (DEBUG) {
		printf("uasLdsKey:\n");
		for (i = 0; i < 16; ++i) {
			printf("%X", uasLdsKey[i]);
		}
		printf("\n");
#ifdef WIN32
		printf("Timestamp (truncated): %u\n", (unsigned int)timestamp);
#else
		printf("Timestamp: %llu\n", timestamp);
#endif
	}
	if (DEBUG) {
		printf("Testing makePacket, packetBuffer:\n");
		printf(" K  L  Value...\n");
		makePacket(packetBuffer);
		for (i = 0; i < 80; ++i) {
			printf("%2X ", packetBuffer[i]);
			if ((i == 15) || (i == 25) || (i == 39) || (i == 53) || (i == 59) || (i == 65) || (i == 69) || (i == 72)) {
				printf("\n");
			}
		}
		printf("\n");
		udpSendPacket((const char *)packetBuffer);
	}
	if (DEBUG) {
		printf("Testing htonll function:\n num: 0x 01 02 03 04 05 06 07 08\n");
		uint64_t number = 0x0102030405060708ULL;
		uint64_t num2 = htonll(number);
		printf("number: ");
		printf("%2X ", (char)(number & 0xFF));
		printf("%2X ", (char)((number & 0xFF00) >> 8));
		printf("%2X ", (char)((number & 0xFF0000) >> 16));
		printf("%2X ", (char)((number & 0xFF000000) >> 24));
		printf("%2X ", (char)((number & 0xFF00000000ULL) >> 32));
		printf("%2X ", (char)((number & 0xFF0000000000ULL) >> 40));
		printf("%2X ", (char)((number & 0xFF000000000000ULL) >> 48));
		printf("%2X ", (char)((number & 0xFF00000000000000ULL) >> 56));
		printf("\n");
		printf("num2: ");
		printf("%2X ", (char)(num2 & 0xFF));
		printf("%2X ", (char)((num2 & 0xFF00) >> 8));
		printf("%2X ", (char)((num2 & 0xFF0000) >> 16));
		printf("%2X ", (char)((num2 & 0xFF000000) >> 24));
		printf("%2X ", (char)((num2 & 0xFF00000000ULL) >> 32));
		printf("%2X ", (char)((num2 & 0xFF0000000000ULL) >> 40));
		printf("%2X ", (char)((num2 & 0xFF000000000000ULL) >> 48));
		printf("%2X ", (char)((num2 & 0xFF00000000000000ULL) >> 56));
		printf("\n");
	}
	// END TESTING==========================================================
	
	while (1) {
		timestamp = htonll(updateTimestamp());
		makePacket(packetBuffer);
		udpSendPacket((const char *)packetBuffer);
		if (DEBUG) {
			printf("\n K  L  Value...\n");
			for (i = 0; i < 80; ++i) {
				printf("%2X ", packetBuffer[i]);
				if ((i == 16) || (i == 26) || (i == 40) || (i == 54) || 
						(i == 60) || (i == 66) || (i == 70) || (i == 73)) {
					printf("\n");
				}
			}
		}
#ifdef WIN32
		Sleep((1 / sendRate) * 1000);
#else
		usleep((1 / sendRate) * 1000); // usleep is untested, may not compile
#endif
	}
}
