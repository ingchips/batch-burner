#include "dev.h"

#include "util/utils.h"
#include "util/usb_utils.h"


SerialDevice::SerialDevice(const char* portName, int buadRate, parity_t parity, bytesize_t databits, stopbits_t stopbits)
	: m_baudRate(buadRate)
	, m_parity(parity)
	, m_databits(databits)
	, m_stopbits(stopbits)
	, m_handle(nullptr)
{
    strcpy(m_portName, portName);
}

bool SerialDevice::open() {
    if (m_handle)
        m_handle->close();
    m_handle = nullptr;

    try {
        m_handle = new Serial(m_portName, m_baudRate,
            serial::Timeout::simpleTimeout(1000), m_databits, m_parity, m_stopbits);
        return m_handle->isOpen();
    } catch (std::exception&) {
        return false;
    }
}

bool SerialDevice::close() {
    if (m_handle)
        m_handle->close();
    m_handle = nullptr;
    return true;
}

bool SerialDevice::isopen()
{
    if (m_handle == nullptr)
        return false;
    return this->m_handle->isOpen();
}


size_t SerialDevice::write(const uint8_t* data, size_t size) 
{
    return m_handle->write(data, size);
}

size_t SerialDevice::read(uint8_t* buffer, size_t size) 
{
    return m_handle->read(buffer, size);
}



USBDevice::USBDevice(const char* deviceName, libusb_device* device)
    : m_vid       (utils::htoi_16(deviceName + 8))
    , m_pid       (utils::htoi_16(deviceName + 17))
    , m_address   (utils::htoi_8(deviceName + 22))
    , m_bus       (utils::htoi_8(deviceName + 25))
    , m_portNumber(utils::htoi_8(deviceName + 28))
    , m_device    (device)
    , m_handle(nullptr)
{
    libusb_ref_device(device);

    strcpy(m_deviceName, deviceName);

    printf("USB#VID_%04X#PID_%04X#%02X#%02X#%02X", m_vid, m_pid, m_address, m_bus, m_portNumber);
}

USBDevice::USBDevice(uint16_t vid, uint16_t pid, uint8_t address, uint8_t bus, uint8_t portNumber, libusb_device* device)
    : m_vid(vid)
    , m_pid(pid)
    , m_address(address)
    , m_bus(bus)
    , m_portNumber(portNumber)
    , m_device(device)
    , m_handle(nullptr)
{
    libusb_ref_device(device);

    sprintf(m_deviceName, "USB#VID_%04X#PID_%04X#%02X#%02X#%02X", vid, pid, address, bus, portNumber);
}

USBDevice::~USBDevice()
{
    libusb_ref_device(m_device);

    close();
}

bool USBDevice::open() 
{
    if (m_handle)
        libusb_close(m_handle);
    m_handle = nullptr;
    if (LIBUSB_SUCCESS != libusb_open(m_device, &m_handle))
        return false;

    if (LIBUSB_SUCCESS != libusb_claim_interface(m_handle, INGCHIPS_KEYBOARD_IINTERFACE))
        return false;

    return false;
}

bool USBDevice::close() 
{
    if (m_handle)
        libusb_close(m_handle);
    m_handle = nullptr;
    return true;
}
bool USBDevice::isopen()
{
    if (m_handle == nullptr)
        return false;
    return true;
}

size_t USBDevice::write(const uint8_t* data, size_t size)
{
    int len = 0;
    if (LIBUSB_SUCCESS != libusb_bulk_transfer(m_handle, INGCHIPS_KEYBOARD_EP_OUT,
        const_cast<uint8_t*>(data), (int)size, &len, DEFAULT_WRITE_TIMEOUT))
        return std::numeric_limits<size_t>::max();
    return (size_t)len;
}

size_t USBDevice::read(uint8_t* buffer, size_t size)
{
    int len = 0;
    if (LIBUSB_SUCCESS != libusb_bulk_transfer(m_handle, INGCHIPS_KEYBOARD_EP_IN,
        const_cast<uint8_t*>(buffer), (int)size, &len, DEFAULT_WRITE_TIMEOUT))
        return std::numeric_limits<size_t>::max();
    return (size_t)len;
}




