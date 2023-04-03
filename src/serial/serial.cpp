
#include <iostream>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include "serial/serial.h"
#include "../util/INIReader.h"

// OS Specific sleep
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define DEF_BAUD 115200
#define RAM_BASE_ADDR 0x20000000

uint8_t auchCRCHi[] = {
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

uint8_t auchCRCLo[] = {
        0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04,
        0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8,
        0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
        0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3, 0x11, 0xD1, 0xD0, 0x10,
        0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
        0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
        0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C,
        0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0,
        0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
        0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
        0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C,
        0xB4, 0x74, 0x75, 0xB5, 0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
        0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54,
        0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98,
        0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
        0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};

std::filesystem::path project_path;

void my_sleep(unsigned long milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds); // 100 ms
#else
    usleep(milliseconds * 1000); // 100 ms
#endif
}

long long get_current_system_time_ms()
{
    std::chrono::system_clock::time_point time_point_now = std::chrono::system_clock::now();
    std::chrono::system_clock::duration duration_since_epoch = time_point_now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch).count();
}
long long get_current_system_time_s()
{
    std::chrono::system_clock::time_point time_point_now = std::chrono::system_clock::now();
    std::chrono::system_clock::duration duration_since_epoch = time_point_now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration_since_epoch).count();
}

uint16_t calc_crc_16(uint8_t* PData, uint32_t size)
{
    uint8_t uchCRCHi = 0xFF;
    uint8_t uchCRCLo = 0xFF;

    for (uint32_t i = 0; i < size; ++i) {
        uint8_t uIndex = uchCRCHi ^ PData[i];
        uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex];
        uchCRCLo = auchCRCLo[uIndex];
    }

    return (uchCRCHi << 8) | uchCRCLo;
}


serial::bytesize_t get_bytesize(int bytesize)
{
    if (bytesize == 8)
        return serial::eightbits;
    else if (bytesize == 7)
        return serial::sevenbits;
    else if (bytesize == 6)
        return serial::sixbits;
    else if (bytesize == 5)
        return serial::fivebits;
    else
        return serial::eightbits;
}

serial::parity_t get_parity(const char* parity)
{
    if (strcmp("even", parity) == 0)
        return serial::parity_even;
    else if (strcmp("odd", parity) == 0)
        return serial::parity_odd;
    else if (strcmp("mark", parity) == 0)
        return serial::parity_mark;
    else if (strcmp("space", parity) == 0)
        return serial::parity_space;
    else
        return serial::parity_none;
}

serial::stopbits_t get_stopbits(const char* stopbits)
{
    if (strcmp("1", stopbits) == 0)
        return serial::stopbits_one;
    else if (strcmp("2", stopbits) == 0)
        return serial::stopbits_two;
    else if (strcmp("1.5", stopbits) == 0)
        return serial::stopbits_one_point_five;
    else
        return serial::stopbits_one;
}

serial::Serial* open_serial(std::string port, int baud, int timeout = 1000, int byteSize = 8,
    std::string parity = "", std::string stopbits = "1")
{
    serial::Serial* my_serial = new serial::Serial(port,
        baud,
        serial::Timeout::simpleTimeout(timeout),
        get_bytesize(byteSize),
        get_parity(parity.c_str()),
        get_stopbits(stopbits.c_str())
    );
    return my_serial->isOpen() ? my_serial : NULL;
}

void close_serial(serial::Serial* serial)
{
    if (serial == NULL)
        return;
    delete serial;
}

#define BOOT_HELLO      "UartBurnStart\n"
#define PAGE_SIZE       8 * 1024

#define ACK             "#$ack\n"
#define NACK            "#$nak\n"
#define STATUS_LOCKED   "#$lck\n"
#define STATUS_UNLOCKED "#$ulk\n"

#define RSP_LEN         6       

#define SEND_PAGE       "#$start"
#define READ_PAGE       "#$readd"
#define SET_ENTRY       "#$setja"
#define CMD_LAUNCH      "#$jumpp"
#define CMD_JUMPD       "#$jumpd"
#define CMD_SET_BAUD    "#$sbaud"
#define CMD_LOCK        "#$lockk"
#define CMD_UNLOCK      "#$unlck"
#define CMD_QLOCKSTATE  "#$state"

uint32_t get_file_size(std::filesystem::path filename)
{
    std::ifstream file;

    file.open(filename, std::ios::in);
    if (!file.is_open())
        return -1;

    file.seekg(0, std::ios_base::end);
    std::streampos filesize = file.tellg();
    file.seekg(0, std::ios_base::beg);

    file.close();

    return filesize;
}

void hex2string_1(uint8_t c, uint8_t* out_buffer)
{
    uint8_t msb = (c & 0xF0) >> 4;
    uint8_t lsb = (c & 0x0F);

    if (msb < 10)
        out_buffer[0] = '0' + msb;
    else
        out_buffer[0] = 'a' + msb - 10;

    if (lsb < 10)
        out_buffer[1] = '0' + lsb;
    else
        out_buffer[1] = 'a' + lsb - 10;
}


std::string uint_to_hex_little(int v, int n)
{
    std::stringstream ss;

    uint8_t buf[2];
    for (uint32_t i = 0; i < n; ++i) {
        uint8_t b = (v >> (i * 8));
        hex2string_1(b, buf);
        ss << '\\' << 'x' << buf[0] << buf[1];
    }
    return ss.str();
}

void append_32_little(uint32_t v, uint8_t* outbuf)
{
    outbuf[0] = v >> 0;
    outbuf[1] = v >> 8;
    outbuf[2] = v >> 16;
    outbuf[3] = v >> 24;
}
void append_16_little(uint16_t v, uint8_t* outbuf)
{
    outbuf[0] = v >> 0;
    outbuf[1] = v >> 8;
}

void append_str(const char* str, uint8_t* outbuf)
{
    memcpy(outbuf, str, strlen(str));
}


std::string exec_cmd(serial::Serial* ser, uint8_t* cmd, uint32_t size)
{
    size = ser->write(cmd, size);
    return ser->read(RSP_LEN);
}


std::string exec_cmd(serial::Serial* ser, std::string cmd)
{
    ser->write(cmd);
    return ser->read(RSP_LEN);
}

bool is_locked(serial::Serial* ser)
{
    std::string rsp = exec_cmd(ser, (uint8_t*)CMD_QLOCKSTATE, 7);

    if (rsp == STATUS_LOCKED)
        return true;
    else if (rsp == STATUS_UNLOCKED)
        return false;
    else
        throw std::exception(("bad response:" + rsp).c_str());
}

bool unlock(serial::Serial* ser)
{
    return exec_cmd(ser, (uint8_t*)CMD_UNLOCK, 7) == ACK;
}
bool lock(serial::Serial* ser)
{
    return exec_cmd(ser, (uint8_t*)CMD_LOCK, 7) == ACK;
}

bool set_baud(serial::Serial* ser, int baud)
{
    uint8_t buf[11] = { 0 };
    append_str(CMD_SET_BAUD, buf + 0);
    append_32_little(baud, buf + 7);
    return exec_cmd(ser, buf, 11) == ACK;
}

bool direct_jump(serial::Serial* ser, int entry_addr)
{
    std::cout << "jump entry" << std::endl;
    uint8_t buf[11] = { 0 };
    append_str(CMD_JUMPD, buf + 0);
    append_32_little(entry_addr, buf + 7);
    return exec_cmd(ser, buf, 11) == ACK;
}
bool set_entry(serial::Serial* ser, int entry_addr)
{
    std::cout << "set entry" << std::endl;
    uint8_t buf[11] = { 0 };
    append_str(SET_ENTRY, buf + 0);
    append_32_little(entry_addr, buf + 7);
    return exec_cmd(ser, buf, 11) == ACK;
}


void launch_app(serial::Serial* ser)
{
    std::cout << "launch" << std::endl;
    ser->write((uint8_t*)CMD_LAUNCH, 7);
}


bool send_page(serial::Serial* ser, int addr, uint8_t* data, uint32_t offset, uint32_t size)
{
    uint8_t buf[14] = { 0 };
    if (addr < RAM_BASE_ADDR)
        append_str(SEND_PAGE, buf + 0);
    else
        append_str("#$stram", buf + 0);
    append_32_little(addr + offset, buf + 7);
    append_16_little(size, buf + 11);

    std::string rsp = exec_cmd(ser, buf, 13);
    if (rsp != ACK)
        return false;

    ser->write(data + offset, size);
    uint16_t crc = calc_crc_16(data + offset, size);
    append_16_little(crc, buf + 0);
    ser->write(buf, 2);
    rsp = ser->read(RSP_LEN);
    return rsp == ACK;
}

bool send_file(serial::Serial* ser, int addr, uint8_t* data, uint32_t size)
{
    uint32_t offset = 0;

    while (size > offset) {
        uint32_t seg = size - offset;
        if (seg > PAGE_SIZE)
            seg = PAGE_SIZE;

        if (!send_page(ser, addr, data, offset, seg))
            return false;

        offset += seg;

        std::cout << ((float)offset / size) * 100 << "%" << std::endl;
    }
    return true;
}

bool wait_handshaking3(serial::Serial* ser, int timeout, std::string hello)
{
    std::cout << "wait for handshaking..." << std::endl;
    long long start = get_current_system_time_s();

    uint8_t acc[1024] = { 0 };
    uint32_t index = 0;
    uint32_t hello_len = hello.size();


    while (true) {
        size_t i = ser->read(acc + index, 1);
        index += i;

        if (index >= hello_len) {
            if (memcmp(hello.data(), (acc + index - hello_len), hello_len) == 0) {
                std::cout << "                               " << std::endl;
                return true;
            }
        }
        if (get_current_system_time_s() - start > timeout) {
            std::cout << std::string((char*)acc) << std::endl;
            return false;
        }
    }
}

bool wait_handshaking(serial::Serial* ser, int timeout)
{
    return wait_handshaking3(ser, timeout, BOOT_HELLO);
}

void do_test(serial::Serial* ser, INIReader& config)
{
}

void ing918_run(serial::Serial* serial, INIReader& config, int timeout, int counter, void* user_data)
{
    serial->flushInput();
    if (!wait_handshaking(serial, timeout)) {
        std::cout << "handshaking timeout" << std::endl;
        return;
    }
    std::cout << "handshaking ok" << std::endl;

    serial::Timeout simple_timeout(timeout * 1000);
    serial->setTimeout(simple_timeout);

    if (is_locked(serial)) {
        if (config.GetBoolean("options", "protection.unlock", false))
            unlock(serial);
        else {
            std::cout << "flash locked" << std::endl;
            return;
        }
    }

    int uart_baud = config.GetInteger("uart", "Baud", DEF_BAUD);
    if (uart_baud != DEF_BAUD) {
        std::cout << "baud -> " << uart_baud << std::endl;
        if (!set_baud(serial, uart_baud)) {
            return;
        }
        my_sleep(100);
        serial->setBaudrate(uart_baud);
    }

    for (uint32_t i = 0; i < 6; ++i)
    {
        std::string section("bin-" + std::to_string(i));

        if (!config.GetBoolean(section, "Checked", 0))
            continue;

        std::string filename_ = config.Get(section, "filename", "");


        std::filesystem::path filename(project_path.u8string() + "/" + filename_);

        if (!std::filesystem::is_regular_file(filename))
            continue;

        int addr = config.GetInteger(section, "address", 0);
        std::cout << "downloading " << filename_ << " 0x" << std::hex << addr << std::dec << std::endl;

        std::vector<uint8_t> data(get_file_size(filename));

        std::ifstream inFile;
        inFile.open(filename, std::ios::in | std::ios::binary);
        if (!inFile.is_open()) continue;
        while (inFile.read((char*)data.data(), data.size()));
        inFile.close();

        if (!send_file(serial, addr, data.data(), data.size()))
            return;
    }

    if (config.GetBoolean("options", "set-entry", true)) {
        int entry_addr = config.GetInteger("options", "entry", 0);

        if (entry_addr >= RAM_BASE_ADDR)
            direct_jump(serial, entry_addr);
        else
            if (!set_entry(serial, entry_addr))
                return;
    }

    if (config.GetBoolean("options", "protection.enabled", false))
        if (!lock(serial))
            return;

    if (config.GetBoolean("options", "launch", true))
        launch_app(serial);

}

void run_proj(std::string proj, bool go = false, int timeout = 5, int counter = -1, void* user_data = NULL)
{
    if (!std::filesystem::is_regular_file(proj))
        return;

    std::filesystem::path path(proj);
    project_path = path.parent_path();


    INIReader reader(proj);

    if (reader.ParseError() < 0) {
        std::cout << "Can't load file\n" << std::endl;
        return;
    }

    serial::Serial* ser = open_serial(
        reader.Get("uart", "Port", "COM0"),
        DEF_BAUD,
        0,
        reader.GetInteger("uart", "DataBits", 8),
        reader.Get("uart", "Parity", ""),
        reader.Get("uart", "StopBits", "1")
    );

    if (ser == NULL)
        return;

    std::string family = reader.Get("main", "family", "ing918");
    if (family == "ing918") {
        ing918_run(ser, reader, timeout, counter, user_data);
    }

    close_serial(ser);
}

int run(int argc, char** argv)
{
    std::string arg1(argv[1]);
    std::cout << arg1 << std::endl;

    if (arg1.find("proj") != 0) {
        exit(0);
    }

    uint32_t i = arg1.find_first_of('=');
    std::string proj = arg1.substr(i + 1);

    std::cout << proj << std::endl;

    run_proj(proj);

    return 0;
}
