#pragma once
#include <iostream>
#include <string>
#include <codecvt>
#include <filesystem>
#include <mutex>
#include "serial/serial.h"
#include "Python.h"

#define DEFAULT_PYTHON_HOME "C:/Program Files/Python310"

class engine
{
public:
	static void setPythonHome(std::string python_home);
	static engine* getInstance();
	static void deleteInstance();

	bool OnStartBin(int batch_counter, int bin_index, std::vector<uint8_t>& data, std::vector<uint8_t>& out_data);


private:
	engine();
	~engine();

	engine(const engine& signal);
	const engine& operator=(const engine& signal);

	static std::string s_PythonHome;
	static engine* s_Instance;
	static std::mutex s_Mutex;

	PyObject* pModule;
	PyObject* pFunc;
};

namespace utils {

	void Alert(bool show, const char* title, const char* text);
	bool Confirm(bool show, const char* title, const char* text);

	void readFileData(std::filesystem::path path, void* out_data);
	std::string readFileText(std::filesystem::path path);

	uint8_t htoi_4(const char c);
	uint8_t htoi_8(const char* c);
	uint16_t htoi_16(const char* c);
	uint32_t htoi_32(const char* c);

	std::string dec2hex(uint32_t i);
	void printf_hexdump(uint8_t* data, uint16_t size);

	std::wstring utf8_to_wstr(const std::string& src);
	std::string wstr_to_utf8(const std::wstring& src);
	std::string utf8_to_gbk(const std::string& str);
	std::string gbk_to_utf8(const std::string& str);
	std::wstring gbk_to_wstr(const std::string& str);

	void my_sleep(unsigned long milliseconds);

	long long get_current_system_time_ms();
	long long get_current_system_time_s();

	void logToFile(const std::string& message, const std::string& filename = "log.txt");
}
