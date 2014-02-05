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
//   gcc -Wall -o klvgen.exe klvgen.c -D WIN32 -lwsock32
//
// Compilation for UNIX:
//   make (requires included Makefile)
// 		OR
//   gcc -Wall -g -o klvgen -lrt main.c
//
// Example usage: ./udpGen -a 127.0.0.1 -p 9000 -r 1 -m "Mission 01" -n "Demo" -t 45.2 -g -93 -e 200
//
// Author: Kevan Ahlquist
// All rights reserved
//============================================================================

#include <getopt.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef WIN32
#	include <windows.h>
#	include <Winsock.h>
#	include <Winsock2.h>
#	include <Ws2tcpip.h>
#	else
#	include <arpa/inet.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#endif
#ifdef __MACH__
#   include <mach/clock.h>
#   include <mach/mach.h>
#endif

//============================================================================

char address[16];
char missionId[13];
char platform[13];
int DEBUG;
float sendRate;
int servPort;
#ifdef WIN32
WSADATA wsaData;
SOCKET sock;
#else
int sock;
#endif

uint16_t altitude; // Map 0..(2^16-1) to -900..19000 meters. 
uint32_t checksum;
int32_t latitude; // map -(2^31-1)..(2^31-1) to +/- 90, Error Indicator: -(2^31) From MISB 601.2
int32_t longitude; //Map -(2^31-1)..(2^31-1) to +/-180. Error Indicator: -(2^31)
uint64_t timestamp;

const unsigned char ldsVersion = 0x02; 	//ldsVersion and uasLdsKey from MISB 601.2 spec
const unsigned char uasLdsKey[] = {0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01,
															0x0E, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00};
const int PACKET_LENGTH = 78;
const char ttl = 64;

unsigned char msgLength = 0x3D;
unsigned char packetBuffer[79];
// Entries in the form {Tag, Length}, Tag is specified in MISB 601.2, Length is BER short form
unsigned char timestampTagLen[] = {0x02, 0x08};
unsigned char missionTagLen[] = {0x03, 0x0C};
unsigned char platformTagLen[] = {0x0A, 0x0C};
unsigned char latitudeTagLen[] = {0x0D, 0x04};
unsigned char longitudeTagLen[] = {0x0E, 0x04};
unsigned char altitudeTagLen[] = {0x0F, 0x02};
unsigned char versionTagLen[] = {0x41, 0x01};
unsigned char checksumTagLen[] = {0x01, 0x02};

struct sockaddr_in servaddr;

//============================================================================
// FUNCTIONS
//--------------------------------------------------
// Maps a value in an input range to a scaled value in the output range
int32_t mapValue(float val, float inStart, float inEnd, float outStart, float outEnd) {
	return (int32_t)(outStart + (((outEnd - outStart) / (inEnd - inStart)) * (val - inStart)));
}
//--------------------------------------------------
// Map -(2^31-1)..(2^31-1) to +/-90. 
int32_t mapLatitude(char *str) {
	float lat = atof(str);
	return (int32_t)mapValue(lat, -90.0, 90.0, -2147483647.0, 2147483647.0); // 2147483647 = (2^31 - 1)
}

//--------------------------------------------------
// Map -(2^31-1)..(2^31-1) to +/-180. 
int32_t mapLongitude(char *str) {
	float lon = atof(str);
	return (int32_t)mapValue(lon, -180.0, 180.0, -2147483647, 2147483647);
}

//--------------------------------------------------
// Map 0..(2^16-1) to -900..19000 meters. 
uint16_t mapAltitude(char *str) {
	int alt = atoi(str);
	return (uint16_t)mapValue(alt, -900, 19000, 0, 65535);
}

//--------------------------------------------------
// Initialize UDP socket
int udpInit(void) {
	//printf("udpInit: servPort: %d\n", servPort);
#ifdef WIN32
	int error;
	error = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (error != 0) {
		perror("Unable to initialize WinSock DLL");
		return 1;
	}
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) {
		//printf("Unable to create socket.");
		perror("Unable to create socket.");
		return -1;
	}
#else
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("Unable to create socket.");
		return -1;
	}
#endif
//	if (setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
//		perror("Unable to set TTL for socket");
//	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(address);
	servaddr.sin_port = htons(servPort);

	//printf("Current servPort: %d, sin_port: %d\n", servPort, servaddr.sin_port);
	return 0;
}

//--------------------------------------------------
// Sends the contents of the given packet, currently fixed length
int udpSendPacket(const char * packet) {
	if (sendto(sock, packet, PACKET_LENGTH, 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
		perror("Error sending socket message");
		return -1;
	}
	return 1;
}

//--------------------------------------------------
// Checksum algorithm from MISB 601.2, pg. 12
uint16_t makeChecksum(unsigned char *buff, unsigned short len) {
	uint16_t bcc = 0, i;
	for ( i = 0 ; i < len; i++) 
    bcc += buff[i] << (8 * ((i + 1) % 2)); 
  return bcc;
}

//--------------------------------------------------
// Check if the system is big endian or not. 
int sysIsBigEndian(void) {
	union {
		uint32_t i;
		char ch[4];
	} tmp = {0x01020304};
	
	return tmp.ch[0] == 1;
}

//--------------------------------------------------
// Convert ints of type uint64_t to network order, checks if conversion is needed.
uint64_t htonll(uint64_t num) {
	if (sysIsBigEndian()) return num;
	else return (((num & 0xFFULL) << 56) | ((num & 0xFF00000000000000ULL) >> 56) |
								((num & 0xFF00ULL) << 40) | ((num & 0x00FF000000000000ULL) >> 40) |
								((num & 0xFF0000ULL) << 24) | ((num & 0x0000FF0000000000ULL) >> 24) |
								((num & 0xFF000000ULL) << 8) | ((num & 0x000000FF00000000ULL) >> 8));
}

//--------------------------------------------------
// Assemble packet in the given buffer
// Will write all fields, to make later modifications easier
// If we need more efficiency, static fields can be initialized on startup, then change only the timestamp.
void makePacket(unsigned char *buff) {
	// Copy all fields into the packet buffer
	memcpy(&buff[0], &uasLdsKey, 16);
	memcpy(&buff[16], &msgLength, 1);
	memcpy(&buff[17], &timestampTagLen, 2);
	memcpy(&buff[19], &timestamp, 8);
	memcpy(&buff[27], &missionTagLen, 2);
	memcpy(&buff[29], &missionId, 12);
	memcpy(&buff[41], &platformTagLen, 2);
	memcpy(&buff[43], &platform, 12);
	memcpy(&buff[55], &latitudeTagLen, 2);
	memcpy(&buff[57], &latitude, 4);
	memcpy(&buff[61], &longitudeTagLen, 2);
	memcpy(&buff[63], &longitude, 4);
	memcpy(&buff[67], &altitudeTagLen, 2);
	memcpy(&buff[69], &altitude, 2);
	memcpy(&buff[71], &versionTagLen, 2);
	memcpy(&buff[73], &ldsVersion, 1);
	memcpy(&buff[74], &checksumTagLen, 2);
	//calculate checksum on buffer
	checksum = makeChecksum(buff, 76);
	memcpy(&buff[76], &checksum, 2);
	return;
}

//--------------------------------------------------
// Returns the current UNIX timestamp in microseconds
uint64_t updateTimestamp(void) {
#ifdef WIN32
	SYSTEMTIME st, epochs;
	FILETIME ft, epochf;
	ULARGE_INTEGER epoch, now;
	
	GetSystemTime(&st);
	epochs.wYear = 1970;
	epochs.wMonth = 1;
	//epochs.wDayOfWeek = ??;
	epochs.wDay = 1;
	epochs.wHour = 0;
	epochs.wMinute = 0;
	epochs.wSecond = 0;
	epochs.wMilliseconds = 0;
	
	SystemTimeToFileTime(&st, &ft);
	SystemTimeToFileTime(&epochs, &epochf);
	
	memcpy(&epoch, &epochf, sizeof(epochf));
	memcpy(&now, &ft, sizeof(ft));
	if (now.QuadPart > epoch.QuadPart) {
		return (uint64_t)((now.QuadPart - epoch.QuadPart) / 10);
	}
	else return 0;
#elif defined __gnu_linux__
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ((((uint64_t)ts.tv_sec * 1000000)) + (((uint64_t)ts.tv_nsec / 1000)));
#elif (defined __APPLE__) && (defined __MACH__)
    struct timespec ts;
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts.tv_sec = mts.tv_sec;
    ts.tv_nsec = mts.tv_nsec;
    return (uint64_t)((ts.tv_sec * 10^9) + ts.tv_nsec);
#endif
}
//--------------------------------------------------
// Displays help information for the tool
void help(void) {
	printf("Usage: klvgen -a <address> -p <port> -r <rate> ...\n");
	printf("  -a or --address <address>\n\tDestination address in dotted quad notation (e.g. 127.0.0.1)\n\tDefault: 127.0.0.1\n");
	printf("  -p or --port <port>\n\tThe port to send packets to\n\tDefault: 9000\n");
	printf("  -r or --rate <rate>\n\tPackets per second (e.g. rate = 30, 30 packets sent per second)\n\tDefault: 1\n");
	printf("  -m or --mission-id <mission-id>\n\t\tMission ID, limited to 12 ASCII characters\n\tDefault: Mission 01\n");
	printf("  -n or --platform <platform>\n\tThe platform name, limited to 12 ASCII characters\n\tDefault: Demo\n");
	printf("  -t or --latitude <latitude>\n\tSensor latitude, given in degrees (e.g. for 35.7S, enter-35.7\n\tDefault: 44.64423\n");
	printf("  -g or --longitude <longitude>\n\tSensor longitude, given in degrees (e.g. for 93.2W, enter-93.2\n\tDefault: -93.24013\n");
	printf("  -e or --altitude <altitude>\n\tSensor altitude, given in meters\n\tDefault: 333\n");
}
//--------------------------------------------------
// Closes UDP socket before exiting
void exitProgram() {
#ifdef WIN32
	closesocket(sock);
	WSACleanup();
#else
	close(sock);
#endif
	//printf("Exiting now...\n");
	exit(0);
}
