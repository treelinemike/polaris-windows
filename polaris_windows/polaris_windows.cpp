#include <stdio.h>
//#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <serial/serial.h>  // cross-platform serial library https://github.com/wjwwood/serial
#include <windows.h>  // for Sleep()
#include "crc16arc.h"

#define POLARIS_DEBUG true

using namespace std;
using namespace serial;

/* Send a Command to the polaris */
int sendCmd(Serial* port, const void* ascii_cmd) {
	
	// add crc-16/arc to end of ascii command
	char* ascii_cmd_char = (char*)ascii_cmd;
	char crc_result[5];
	char* full_cmd;
	full_cmd = (char*)malloc(strlen(ascii_cmd_char) + 4);
	memcpy(full_cmd, ascii_cmd_char, strlen(ascii_cmd_char));
	char* full_cmd_ptr = full_cmd + strlen(ascii_cmd_char);
	sprintf_s(crc_result, "%04X", crc16arc_byte((uint16_t)0x0000, ascii_cmd_char, strlen(ascii_cmd_char)));
	memcpy(full_cmd_ptr, crc_result, 4);
	full_cmd_ptr += 4;
	*full_cmd_ptr = '\0';	

#if POLARIS_DEBUG
	printf("Raw command string: %s\r\n", ascii_cmd_char);
	printf("Command string with CRC: %s\r\n", full_cmd);
#endif

	// write to serial port if it is valid
	// skip if nullptr to allow CRC check functionality with POLARIS_DEBUG set to true
	if (port != nullptr) {
		*(full_cmd_ptr) = '\r';  // replace \0 with \r for transmission
		*(full_cmd_ptr++) = '\0';
		port->write(full_cmd);
	}

	// done
	return 0;
}

int main(void) {

	// test CRC
	cout << "Testing CRC:" << endl;
	Serial* null_port = NULL;
	sendCmd(null_port, "RESET");

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
	std::string myPort("COM5");
	Serial*  mySerialPort = NULL;
	try {
		mySerialPort = new Serial(myPort, 9600U, Timeout(100, 100, 100, 100, 100), eightbits, parity_none, stopbits_one, flowcontrol_none);
	}
	catch (IOException e) {
		cout << e.what();
		return -1;
	};
	mySerialPort->flush();


	// equiv of serial break... it works!
	cout << "Sending serial break..." << endl;
	mySerialPort->setBreak(true);
	Sleep(10);
	mySerialPort->setBreak(false);

	// wait a bit, then read status
	Sleep(1000);
	string readData = mySerialPort->read(size_t(1000));
	cout << "Read: " << readData << endl;

	// wait a bit longer
	// TODO: figure out why we need this...
	Sleep(1000);

	// send a beep command
	cout << "Sending BEEP command..." << endl;
	sendCmd(mySerialPort,"BEEP:9");
	readData = mySerialPort->read(size_t(1000));
	cout << "Read: " << readData << endl;

	// close the serial port
	mySerialPort->close();

	// report success
	cout << "Done!" << endl;

	// return
	return 0;
}