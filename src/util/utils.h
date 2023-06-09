#pragma once
#include <iostream>
#include <string>
#include <codecvt>
#include <filesystem>
#include <mutex>
#include "serial/serial.h"
#include "Python.h"

class engine
{
public:
	static engine* getInstance();
	static void deleteInstance();

	bool OnStartBin(int batch_counter, int bin_index, std::vector<uint8_t>& data, std::vector<uint8_t>& out_data);


private:
	engine();
	~engine();

	engine(const engine& signal);
	const engine& operator=(const engine& signal);

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

	std::string dec2hex(uint32_t i);

	std::wstring utf8_to_wstr(const std::string& src);
	std::string wstr_to_utf8(const std::wstring& src);
	std::string utf8_to_gbk(const std::string& str);
	std::string gbk_to_utf8(const std::string& str);
	std::wstring gbk_to_wstr(const std::string& str);

	void my_sleep(unsigned long milliseconds);

	long long get_current_system_time_ms();
	long long get_current_system_time_s();
}
