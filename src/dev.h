#ifndef __DEV_H__
#define __DEV_H__
/**
 *         ┏┓   ┏┓+ +
 *        ┏┛┻━━━┛┻┓ + +
 *        ┃       ┃
 *        ┃   ━   ┃ ++ + + +
 *        ████━████ +
 *        ┃       ┃ +
 *        ┃   ┻   ┃
 *        ┃       ┃ + +
 *        ┗━┓   ┏━┛
 *          ┃   ┃
 *          ┃   ┃ + + + +
 *          ┃   ┃    Code is far away from bug with the animal protecting
 *          ┃   ┃ +  神兽保佑,代码无bug
 *          ┃   ┃
 *          ┃   ┃  +
 *          ┃   ┗━━━┓ + +
 *          ┃       ┣┓
 *          ┃       ┏┛
 *          ┗┓┓┏━┳┓┏┛ + + + +
 *           ┃┫┫ ┃┫┫
 *           ┗┻┛ ┗┻┛+ + + +
 *
 * @author chxuang
 * @date 2023年9月10日12:10:293
 */
#include "pch.h"

#include <iostream>
#include <string>

#include "libusb-1.0/libusb.h"
#include "serial/serial.h"

using namespace serial;

class Device {
public:
    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual bool isopen() = 0;
    virtual size_t write(const uint8_t* data, size_t size) = 0;
    virtual size_t read(uint8_t* buffer, size_t size) = 0;
    virtual ~Device() {}
};

class SerialDevice : public Device {
public:
    SerialDevice(const char* portName, int buadRate = DEFAULT_BAUDRATE, parity_t parity = parity_none, bytesize_t databits = eightbits, stopbits_t stopbits = stopbits_one);

    bool open() override;

    bool close() override;

    bool isopen() override;

    size_t write(const uint8_t* data, size_t size) override;

    size_t read(uint8_t* buffer, size_t size) override;

private:
    char        m_portName[32];
    int         m_baudRate;
    parity_t    m_parity;
    bytesize_t  m_databits;
    stopbits_t  m_stopbits;
    Serial*     m_handle;
};

class USBDevice : public Device {
public:
    USBDevice(const char* deviceName, libusb_device* device);
    USBDevice(uint16_t vid, uint16_t pid, uint8_t address, uint8_t bus, uint8_t portNumber, libusb_device* device);
    ~USBDevice();

    bool open() override;

    bool close() override;

    bool isopen() override;

    size_t write(const uint8_t* data, size_t size) override;

    size_t read(uint8_t* buffer, size_t size) override;

private:
    char                    m_deviceName[32];
    uint16_t                m_vid;
    uint16_t                m_pid;
    uint8_t                 m_bus;
    uint8_t                 m_address;
    uint8_t                 m_portNumber;
    libusb_device*          m_device;
    libusb_device_handle*   m_handle;
};

class DeviceFactory {
public:
    static Device* createSerialDevice(const char* portName) {
        return new SerialDevice(portName);
    }

    static Device* createUSBDevice(const char* deviceName, libusb_device* device) {
        return new USBDevice(deviceName, device);
    }
};



#endif//__DEV_H__