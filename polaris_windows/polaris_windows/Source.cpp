#include <stdio.h>
#include <iostream>
#include <string>
#include <serial/serial.h>  // cross-platform serial library https://github.com/wjwwood/serial
#include <windows.h>  // for Sleep()
using namespace std;
using namespace serial;

int main(void){
	
	// open the serial port
	std::string myPort("COM5");
	Serial mySerialPort(myPort, 9600U, Timeout(10,10,10,10,10), eightbits, parity_none, stopbits_one, flowcontrol_none);
	
	// equiv of serial break... it works!
	mySerialPort.setBreak(true);
	Sleep(10);
	mySerialPort.setBreak(false);

	// close the serial port
	mySerialPort.close();

	// report success
	cout << "Done!" << endl;

	// return
	return 0;
}