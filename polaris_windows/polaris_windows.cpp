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

#define POLARIS_DEBUG true
#define RESP_BUF_SIZE 1024

using namespace std;
using namespace serial;

// function prototypes
int sendPolaris(Serial* port, const void* ascii_cmd);
int readPolaris(Serial* port, char* resp_buffer, unsigned int max_buffer_size, unsigned int& buffer_size);

// main
int main(void) {
	
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
	std::string myPort("COM5");
	Serial*  mySerialPort = NULL;
	try {
		mySerialPort = new Serial(myPort, 9600U, Timeout(50, 200, 3, 200, 3), eightbits, parity_none, stopbits_one, flowcontrol_none);
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
	// need to use std::chrono::sys_clock *NOT* sys::chrono:utc_clock b/c utc_clock includes leap seconds
	// eventually (slightly) modified this from https://stackoverflow.com/questions/16177295/get-time-since-epoch-in-milliseconds-preferably-using-c11-chrono
	// now appears to correspond to MATLAB posixtime(datetime('now','TimeZone','UTC')
	unsigned long long milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now().time_since_epoch()).count();
	double base_unix_timestamp = ((double)milliseconds_since_epoch) / 1000.0F;
	printf("Base UNIX timestamp: %15.4f\r\n",base_unix_timestamp);
	
	// read status of Polaris reset
	char resp_buffer[RESP_BUF_SIZE] = {'\0'};
	unsigned int buff_size;
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// send a beep:1 command
	cout << "Sending BEEP:1 command..." << endl;
	sendPolaris(mySerialPort,"BEEP:1");  //mySerialPort->write(std::string("BEEP:94205\r"));
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// now we need to change baud rate to 57,600
	cout << "Commanding Polaris baud rate change..." << endl;
	sendPolaris(mySerialPort, "COMM:40000");
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// close and re-open serial port at 57,600
	mySerialPort->close();
	Sleep(500);
	try {
		mySerialPort = new Serial(myPort, 57600U, Timeout(50, 200, 3, 200, 3), eightbits, parity_none, stopbits_one, flowcontrol_none);
	}
	catch (IOException e) {
		cout << e.what();
		return -1;
	};
	mySerialPort->flush();

	// send a beep:2 command
	cout << "Sending BEEP:2 command..." << endl;
	sendPolaris(mySerialPort, "BEEP:2");
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// send a init command
	cout << "Sending INIT command..." << endl;
	sendPolaris(mySerialPort, "INIT:");
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// select volume (without querying them first...) command
	cout << "Selecting Polaris volume..." << endl;
	sendPolaris(mySerialPort, "VSEL:1");;
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// set illuminator rate to 20Hz
	cout << "Setting illuminator rate..." << endl;
	sendPolaris(mySerialPort, "IRATE:0");
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// NOW SEND TOOL DEF. FILE(S)
	// test reading in the binary ROM file
	uint16_t bytecount = 0;
	uint8_t tool_id_char = 'A';
	char filebuf[64] = { '\0' }; // binary storage
	char cmdbuf[139] = { '\0' }; // ASCII storage
	char* pCmdbuf = nullptr;
	std::streamsize bytes;
	//std::ifstream romfile("C:\\Users\\f002r5k\\Dropbox\\projects\\surg_nav\\NDI\\Polaris\\Tool Definition Files\\Medtronic_960_556.rom", std::ios::binary);
	std::ifstream romfile("C:\\Users\\f002r5k\\Desktop\\medtronic_9730605_referece.rom", std::ios::binary);
	while (!romfile.eof()) {

		// initialize command string for next segment of ROM file data
		sprintf_s(cmdbuf, "PVWR:%C%04X", tool_id_char,bytecount);
		pCmdbuf = cmdbuf + 10;

		// read up to 64 bytes from ROM file
		// TODO: should ensure that we always get 64 bytes except for at EOF
		// should rework so we read entire ROM into memory first then send to Polaris
		romfile.read(filebuf, 64);
		bytes = romfile.gcount();
		bytecount += bytes;
		if (!romfile.eof())
			assert(bytes == 64,"Didn't read 64 bytes from ROM file!");


		for (unsigned int j = 0; j < bytes; j++) {
			//printf("%02X ", ((uint8_t)filebuf[j]));
			//_itoa_s((uint8_t)filebuf[j], pCmdbuf, 3,16);
			sprintf_s(pCmdbuf, 3, "%02X", (uint8_t)filebuf[j]); // slow! TODO replace with	https://github.com/fmtlib/fmt
			//printf("itoa result <%s>\r\n", pCmdbuf);
			pCmdbuf += 2;
		}

		// pad command buffer with FF
		while (pCmdbuf < (cmdbuf + 138)) {
			sprintf_s(pCmdbuf, 3, "FF");
			pCmdbuf += 2;
		}

		// send current command buffer with ROM file data 
		cout << "Sending ROM file segment..." << endl;
		sendPolaris(mySerialPort, cmdbuf);
		readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
		printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);
	}
	
	// close the ROM file
	romfile.close();
	printf("Read a total of %d bytes from ROM file\r\n", bytecount);

	// initialize port handle
	cout << "Initializing port handle..." << endl;
	sendPolaris(mySerialPort, "PINIT:A");
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// enable tool tracking
	cout << "Enabling tool tracking..." << endl;
	sendPolaris(mySerialPort, "PENA:AD");
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// 1-3 tools, regular tracking mode
	// >> GX_CMD_STR = "GX:800B";
	// >> PSTAT_CMD_STR = "PSTAT:801f";
	//
	// 1-3 tools, tool ID mode
	// >> GX_CMD_STR = "GX:9000";
	// >> PSTAT_CMD_STR = "PSTAT:801f";
	//
	// 1-9 tools, regular tracking mode
	// >> GX_CMD_STR = "GX:A00B";
	// >> PSTAT_CMD_STR = "PSTAT:A01f";

	// send PSTAT command to confirm tool configuration
	// TODO: figure out why tool not listed properly (all zeros and a one)... mabye we're not sending the file correctly?
	cout << "Sending appropriate PSTAT command..." << endl;
	sendPolaris(mySerialPort, "PSTAT:A01F");
	readPolaris(mySerialPort, resp_buffer, RESP_BUF_SIZE, buff_size);
	printf("Read %d chars: <%s>\r\n", buff_size, resp_buffer);

	// close serial port
	mySerialPort->close();

	// report success
	cout << "Done!" << endl;

	// return
	return 0;
}


// Send a Command to the polaris
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

// Read a command from the Polaris
// writes response (without CRC or terminating \r to resp_buffer
// also writes \0 to resp_buffer[buffer_size], so resp_buff needs to be of size (buffer_size+1)
int readPolaris(Serial* port, char* resp_buffer, unsigned int max_buffer_size, unsigned int& buffer_size) {

	// get response from polaris
	std::string polaris_response = port->read(size_t(1024));

	if (polaris_response.back() != '\r') {
		printf("ERROR: RESPONSE DOESN'T END IN <CR>!\r\n"); // TODO: Handle appropriately
		printf("Entire response <%s>\r\n", polaris_response.c_str());
	}

	// copy response into a legit c string
	// replaceing last character (which we know is \r) with null terminator \0
	char* polaris_response_cstr = (char*)malloc(polaris_response.length() + 1);
	if (polaris_response_cstr == NULL) exit(1); // to avoid could be zero warning 
	memcpy(polaris_response_cstr, polaris_response.c_str(), polaris_response.length());
	*(polaris_response_cstr + polaris_response.length()) = '\0';

	if (max_buffer_size >= (polaris_response.length()-4)) { // -4 takes out CRC, leaves room for \0

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