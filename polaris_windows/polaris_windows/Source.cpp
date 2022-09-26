#include <stdio.h>
#include <iostream>
#include <string>
#include <serial/serial.h>  // cross-platform serial library https://github.com/wjwwood/serial
#include <windows.h>  // for Sleep()
using namespace std;
using namespace serial;

int main(void){
	
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
	mySerialPort->write("BEEP 9\r");
	readData = mySerialPort->read(size_t(1000));
	cout << "Read: " << readData << endl;

	// close the serial port
	mySerialPort->close();

	// report success
	cout << "Done!" << endl;

	// return
	return 0;
}