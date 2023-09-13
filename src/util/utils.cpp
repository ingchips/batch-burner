#include "utils.h"
#include <sstream>
#include <vector>
#include <fstream>

#include "imgui.h"
#include <Python.h>
#include "ImConsole.h"

// OS Specific sleep
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

extern ImLogger* logger;

static PyObject* AppendLog(PyObject* self, PyObject* args)
{
	int nType = -1;
	const char* text = NULL;
	if (!PyArg_ParseTuple(args, "s", &text))
		return nullptr;

	logger->AddLog(text);

	//Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef BurnerMethods[] = {
	{"AppendLog", AppendLog, METH_VARARGS, "Append Log"},
	{NULL, NULL, 0, NULL}
};

static PyModuleDef BurnerModule = {
	PyModuleDef_HEAD_INIT, "burner", NULL, -1, BurnerMethods,
	NULL, NULL, NULL, NULL
};

static PyObject* PyInitCppInterface(void)
{
	return PyModule_Create(&BurnerModule);
}



void utils::Alert(bool show, const char* title, const char* text)
{
	if (show)
		ImGui::OpenPopup(title);

	// Always center this window when appearing
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(text);
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}

bool utils::Confirm(bool show, const char* title, const char* text)
{
	if (show)
		ImGui::OpenPopup(title);

	// Always center this window when appearing
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	bool result = false;

	if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(text);
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0))) 
		{ 
			ImGui::CloseCurrentPopup(); 
			result = true;
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) 
		{ 
			ImGui::CloseCurrentPopup(); 
		}
		ImGui::EndPopup();
	}
	return result;
}

void utils::readFileData(std::filesystem::path path, void* out_data)
{
	std::ifstream inFile;
	inFile.open(path, std::ios::in | std::ios::binary);
	while (inFile.read((char*)out_data, std::filesystem::file_size(path)));
	inFile.close();
}

std::string utils::readFileText(std::filesystem::path path)
{
	std::ifstream inFile;
	std::stringstream ss;
	std::string content;
	inFile.open(path, std::ios::in);

	while (!inFile.eof())
	{
		inFile >> content;
		ss << content;
	}
	inFile.close();

	return ss.str();
}


uint8_t utils::htoi_4(const char c)
{
	if ('0' <= c && c <= '9') return c - '0';
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	return 0;
}
uint8_t utils::htoi_8(const char* c)
{
	return (htoi_4(c[0]) << 4) | htoi_4(c[1]);
}
uint16_t utils::htoi_16(const char* c)
{
	return (htoi_8(c) << 8) | htoi_8(c + 2);
}
uint32_t utils::htoi_32(const char* c)
{
	return (htoi_8(c) << 24) | (htoi_8(c + 2) << 16) | (htoi_8(c + 4) << 8) | htoi_8(c + 6);
}

std::string utils::dec2hex(uint32_t i)
{
	std::ostringstream ss;
	ss << "0x" << std::hex << i;
	return ss.str();
}


void utils::printf_hexdump(uint8_t* data, uint16_t size)
{
	std::cout << std::hex;
	for (uint16_t i = 0; i < size; ++i) {
		std::cout << (int)data[i];
	}
	std::cout << std::dec << std::endl;
}


std::wstring utils::utf8_to_wstr(const std::string& src)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.from_bytes(src);
}
std::string utils::wstr_to_utf8(const std::wstring& src)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	return convert.to_bytes(src);
}
std::string utils::utf8_to_gbk(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
	std::wstring tmp_wstr = conv.from_bytes(str);

	//GBK locale name in windows
	const char* GBK_LOCALE_NAME = ".936";
	std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
	return convert.to_bytes(tmp_wstr);
}
std::string utils::gbk_to_utf8(const std::string& str)
{
	//GBK locale name in windows
	const char* GBK_LOCALE_NAME = ".936";
	std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
	std::wstring tmp_wstr = convert.from_bytes(str);

	std::wstring_convert<std::codecvt_utf8<wchar_t>> cv2;
	return cv2.to_bytes(tmp_wstr);
}
std::wstring utils::gbk_to_wstr(const std::string& str)
{
	//GBK locale name in windows
	const char* GBK_LOCALE_NAME = ".936";
	std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
	return convert.from_bytes(str);
}

void utils::my_sleep(unsigned long milliseconds) {
#ifdef _WIN32
	Sleep(milliseconds); // 100 ms
#else
	usleep(milliseconds * 1000); // 100 ms
#endif
}

long long utils::get_current_system_time_ms()
{
	std::chrono::system_clock::time_point time_point_now = std::chrono::system_clock::now();
	std::chrono::system_clock::duration duration_since_epoch = time_point_now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch).count();
}
long long utils::get_current_system_time_s()
{
	std::chrono::system_clock::time_point time_point_now = std::chrono::system_clock::now();
	std::chrono::system_clock::duration duration_since_epoch = time_point_now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::seconds>(duration_since_epoch).count();
}

std::string engine::s_PythonHome = "";
engine* engine::s_Instance = nullptr;
std::mutex engine::s_Mutex;


void engine::setPythonHome(std::string python_home)
{
	s_PythonHome = python_home;
}

engine* engine::getInstance()
{
	if (s_Instance == nullptr) {
		std::unique_lock<std::mutex> lock(s_Mutex);	//lock
		if (s_Instance == nullptr) {
			s_Instance = new engine();
		}
		//unlock
	}
	return s_Instance;
}
void engine::deleteInstance()
{
	std::unique_lock<std::mutex> lock(s_Mutex);	//lock
	if (s_Instance != nullptr) {
		delete s_Instance;
		s_Instance = nullptr;
	}
}


engine::engine()
	:pModule(NULL)
	,pFunc(NULL)
{
	if (PyImport_AppendInittab("burner", PyInitCppInterface) == -1) {
		logger->AddLog("Error: could not extend in-built modules table\n");
		return;
	}

	if (s_PythonHome == "")
	{
		s_PythonHome = std::string(DEFAULT_PYTHON_HOME);
	}

	size_t size = 0;
	PyConfig config;
	PyConfig_InitPythonConfig(&config);
	config.home = Py_DecodeLocale(s_PythonHome.c_str(), &size);
	Py_InitializeFromConfig(&config);
	PyConfig_Clear(&config);

	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append('./')");


	pModule = PyImport_ImportModule("burner");
	if (pModule == NULL) {
		logger->AddLog("Failed to load Python Module: burner\n");
		return;
	}

	pModule = PyImport_ImportModule("intercept");
	if (pModule == NULL) {
		logger->AddLog("Failed to load Python Module: intercept\n");
		return;
	}

	pFunc = PyObject_GetAttrString(pModule, "on_start_bin");
	if (pFunc == NULL) {
		logger->AddLog("Failed to load Python Function: on_start_bin\n");
		return;
	}

	PyEval_SaveThread();	//Release GIL
}

engine::~engine()
{
	Py_Finalize();
}

engine::engine(const engine& signal)
	:pModule(NULL)
	, pFunc(NULL)
{
}
const engine& const engine::operator=(const engine& signal)
{
	return *this;
}

bool engine::OnStartBin(int batch_counter, int bin_index, std::vector<uint8_t>& data, std::vector<uint8_t>& out_data)
{
	PyObject* data0 = PyBytes_FromStringAndSize((const char*)data.data(), data.size());

	PyObject* cons_args = Py_BuildValue("(iiO)", batch_counter, bin_index, data0);
	

	PyObject* obj = PyObject_CallObject(pFunc, cons_args);
	if (obj == NULL) {
		logger->AddLog("Failed to call Python Function: on_start_bin\n");
		return false;
	}

	Py_DECREF(cons_args);

	Py_ssize_t size_tuple = PyTuple_Size(obj);

	PyObject* abort = PyTuple_GetItem(obj, 0);
	PyObject* data_ = PyTuple_GetItem(obj, 1);

	if (Py_True == abort) {
		logger->AddLog("abort");
		return false;
	}

	char* data_after = PyBytes_AsString(data_);
	Py_ssize_t out_size = PyBytes_GET_SIZE(data_);

	out_data.resize(out_size);
	memcpy(out_data.data(), data_after, out_size);

	Py_DECREF(obj);
	Py_DECREF(data0);

	return true;
}