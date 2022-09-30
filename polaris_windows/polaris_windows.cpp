#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <serial/serial.h>  // cross-platform serial library https://github.com/wjwwood/serial
#include <windows.h>  // for Sleep()
#include <chrono>     // for UTC time
#include <format>     // for formatting clock stuff
#include "crc16arc.h"
#include <cassert>

#define POLARIS_DEBUG false

using namespace std;
using namespace serial;

// function prototypes
int sendPolaris(Serial* port, const void* ascii_cmd);
int readPolaris(Serial* port, char* resp_buffer, unsigned int max_buffer_size, unsigned int& buffer_size);

// main
int main(void) {
	
	// test reading in the binary ROM file
	uint32_t bytecount = 0;
	char filebuf[16] = {'\0'};
	std::streamsize bytes;
	std::ifstream romfile("C:\\Users\\f002r5k\\Desktop\\medtronic_9730605_referece.rom", std::ios::binary);
	while (!romfile.eof()) {
		romfile.read(filebuf,16);
		bytes = romfile.gcount();
		bytecount += bytes;
		for (unsigned int j = 0; j < bytes; j++) {
			printf("%02X ", (uint8_t)filebuf[j]);
		}
		printf("\r\n");
	}
	romfile.close();
	printf("Read a total of %d bytes\r\n", bytecount);

	// find available COM ports
	vector<PortInfo> all_ports = list_ports();
	if (all_ports.size() > 0) {
		cout << "Available COM ports:" << endl;
		for (unsigned int i = 0; i < all_ports.size(); i++) {
			cout << "* " << all_ports[i].port << " (" << all_ports[i].description << " / " << all_ports[i].hardware_id << ")" << endl;
		}
		cout << endl;
	}
	else {
		cout << "No COM ports found!" << endl;
		return -1;
	}

	// open the serial port
	cout << "Opening serial port..." << endl;
	std::string myPort("COM3");
	Serial*  mySerialPort = NULL;
	try {
		mySerialPort = new Serial(myPort, 9600U, Timeout(10, 100, 2, 100, 2), eightbits, parity_none, stopbits_one, flowcontrol_none);
	}
	catch (IOException e) {
		cout << e.what();
		return -1;
	};
	mySerialPort->flush();

	// equiv of serial break
	cout << "Sending serial break..." << endl;
	mySerialPort->setBreak(true);
	Sleep(10);
	mySerialPort->setBreak(false);

	// wait a bit for system to reset, then read status
	Sleep(3000);

	// get UNIX UTC timestamp here
	// REQUIRES c++20, set this under project properties>C++>Language
	std::chrono::utc_clock* myClock = nullptr;
	const auto baseUnixTime = myClock->now();
	std::cout << std::format("Current UTC Time: {0:%F} {0:%T}.", baseUnixTime) << endl;
	std::chrono::duration<double> baseUnixTimestamp = baseUnixTime.time_since_epoch();
	printf("Seconds since epoch: %0.4f\r\n", baseUnixTimestamp.count());

/*
	std::chrono::time_point<std::chrono::utc_clock> epochTime;
	std::cout << std::format("UNIX Time Epoch: {0:%F} {0:%T}.", epochTime) << endl;
	std::chrono::time_point<std::chrono::utc_clock> baseUnixTime = myClock->now();
	std::cout << std::format("Base UNIX Timestamp: {0:%F} {0:%T}.", baseUnixTime) << endl;
	std::chrono::duration<long, std::milli> duration;
	*/

	// read status of Polaris reset
	char resp_buffer[256] = { '\0' };
	unsigned int buff_size;
	readPolaris(mySerialPort, resp_buffer, 256, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// send a beep command
	cout << "Sending BEEP command..." << endl;
	sendPolaris(mySerialPort,"BEEP:1");  //mySerialPort->write(std::string("BEEP:94205\r"));
	readPolaris(mySerialPort, resp_buffer, 256, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// close the serial port
	mySerialPort->close();

	// report success
	cout << "Done!" << endl;

	// return
	return 0;
}


/* Send a Command to the polaris */
int sendPolaris(Serial* port, const void* ascii_cmd) {

	// add crc-16/arc to end of ascii command
	char* ascii_cmd_char = (char*)ascii_cmd;
	char crc_result[5]; // four for the crc, and one for \0
	char* full_cmd = (char*)malloc(strlen(ascii_cmd_char) + 6);
	if (full_cmd == NULL) exit(1);  // prevent a "could be zero" warning for malloc and port->write()
	memcpy(full_cmd, ascii_cmd_char, strlen(ascii_cmd_char));  // copy ascii command string to full_cmd
	char* full_cmd_ptr = full_cmd + strlen(ascii_cmd_char); // position a pointer in full_cmd just after what we've written so far
	sprintf_s(crc_result, "%04X", crc16arc_byte((uint16_t)0x0000, ascii_cmd_char, strlen(ascii_cmd_char))); // generate CRC
	memcpy(full_cmd_ptr, crc_result, 4); // copy crc (first four bytes only!) into full_cmd 
	full_cmd_ptr += 4;  // increment pointer just past CRC

#if POLARIS_DEBUG
	*full_cmd_ptr = '\0'; // add a null terminator for display
	printf("Raw command string: %s\r\n", ascii_cmd_char);
	printf("Command string with CRC: %s\r\n", full_cmd);
#endif

	// write to serial port if it is valid
	// skip if nullptr to allow CRC check functionality with POLARIS_DEBUG set to true
	if (port != nullptr) {
		*(full_cmd_ptr) = '\r';    // replace \0 with \r for transmission
		*(++full_cmd_ptr) = '\0';  // importnat to pre-increment full_cmd_ptr!!
		port->write(std::string(full_cmd));
	}

	// done
	return 0;
}

/* Read a command from the Polaris */
int readPolaris(Serial* port, char* resp_buffer, unsigned int max_buffer_size, unsigned int& buffer_size) {

	// get response from polaris
	std::string polaris_response = port->read(size_t(1024));

	if (polaris_response.back() != '\r') {
		printf("ERROR: RESPONSE DOESN'T END IN <CR>!\r\n"); // TODO: Handle appropriately
	}

	// copy response into a legit c string
	// replaceing last character (which we know is \r) with null terminator \0
	char* polaris_response_cstr = (char*)malloc(polaris_response.length() + 1);
	if (polaris_response_cstr == NULL) exit(1); // to avoid could be zero warning 
	memcpy(polaris_response_cstr, polaris_response.c_str(), polaris_response.length());
	*(polaris_response_cstr + polaris_response.length()) = '\0';

	if (polaris_response.length() <= max_buffer_size) {

		// copy everything but CRC and \r to response buffer
		memcpy(resp_buffer, polaris_response_cstr, polaris_response.length() - 5);
		buffer_size = polaris_response.length() - 5;
		*(resp_buffer + buffer_size) = '\0';

		// extract CRC as received
		char crc_received[5] = { '\0' };  // 5 chars b/c we may want to printf()
		memcpy(crc_received, polaris_response_cstr + buffer_size, 4);
		//printf("CRC as received: %s\r\n", crc_received);

		// compute CRC
		unsigned int msg_size = buffer_size - 4;
		char crc_computed[5]; // four for the crc, and one for \0
		//printf("Response w/o CRC: %s \r\n", resp_buffer);
		sprintf_s(crc_computed, "%04X", crc16arc_byte((uint16_t)0x0000, resp_buffer, buffer_size)); // generate CRC
		//printf("CRC Result: %s\r\n", crc_computed);
		int crc_match = strcmp(crc_computed, crc_received);
		if (crc_match != 0) {
			printf("ERROR: CRC MISMATCH!\r\n"); // TODO: HANDLE THIS APPROPRIATELY
			return -2;
		}
	}
	else {
		printf("INADEQUATE BUFFER SIZE!\r\n"); // TODO: HANDLE THIS APPROPRIATELY
		return -1;
	}

	// done
	return 0;
}