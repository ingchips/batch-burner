#include "ingBurner.h"

bool SerialPort::IsOpen()
{
	if (this->ser == NULL)
		return false;

	return this->ser->isOpen();
}

bool SerialPort::CloseSerial()
{
	if (this->ser)
		this->ser->close();
	this->ser = NULL;

	return true;
}

bool SerialPort::OpenSerial()
{
	if (this->ser)
		this->ser->close();
	this->ser = NULL;

	try {
		this->ser = std::make_shared<serial::Serial>(
			this->portName,
			115200,
			serial::Timeout::simpleTimeout(1000),
			this->databits,
			this->parity,
			this->stopbits
		);
		return this->ser->isOpen();
	} catch (std::exception e) {
	}
	return false;
}



