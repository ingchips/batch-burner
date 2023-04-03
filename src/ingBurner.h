#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <sstream>
#include <fstream>
#include "util/INIReader.h"
#include <serial/serial.h>
#include "util/utils.h"

#define BIN_BURN_PLAN_MAX_SIZE 6
#define SERIAL_PORT_MAX_NUM 20

static std::filesystem::path default_config_file = std::filesystem::current_path().concat("\\flash_downloader.ini");

static uint32_t burn_plan_color[BIN_BURN_PLAN_MAX_SIZE] = {
	0xffffe700,
	0xff7eed69,
	0xff00abea,
	0xff0056ff,
	0xff587d48,
	0xff00a78e,
};

static uint32_t port_color[SERIAL_PORT_MAX_NUM] = {
	0xFFFF8080,
	0xFFFFBF80,
	0xFFFFFF80,
	0xFFBFFF80,
	0xFF80FF80,
	0xFF80FFBF,
	0xFF80FFFF,
	0xFF80BFFF,
	0xFF8080FF,
	0xFFBF80FF,
	0xFFFF80FF,
	0xFFFF80BF,
	0xFFFF4242,
	0xFF42FFFF,
	0xFFCC66CC,
	0xFF66CC66,
	0xFFCC9966,
	0xFF66CC99,
	0xFF6666CC,
	0xFF6699CC,
};

typedef enum {
	SERIAL_PORT_UNUSED = 0,
	SERIAL_PORT_INVALID,
	SERIAL_PORT_VALID,
	SERIAL_PORT_READY,
} SerialPortState;

static std::string parity_v2c(serial::parity_t parity)
{
	switch (parity)
	{
	case serial::parity_none: return "none";
	case serial::parity_odd: return "odd";
	case serial::parity_even: return "even";
	case serial::parity_mark: return "mark";
	case serial::parity_space: return "space";
	default:
		throw std::exception("unknown parity");
	}
}
static serial::parity_t parity_c2v(std::string parity)
{
	if (parity == "odd")	return serial::parity_none;
	if (parity == "even")	return serial::parity_even;
	if (parity == "mark")	return serial::parity_mark;
	if (parity == "space")	return serial::parity_space;
	return serial::parity_none;
}


struct Options {
	bool download;
	bool verify;
	bool redownload;
	int entry;
	bool set_entry;
	bool launch;
	bool batch;
	int batch_current;
	int batch_limit;
	bool protection_enabled;
	bool protection_unlock;
	bool resetReservedFlash;
	bool UseScript;
	char script[4096];
};

typedef std::shared_ptr<serial::Serial> serial_handle_sp_t;

struct SerialPort {
	bool enable;
	char portName[32];
	char baudText[32];
	int baud;
	SerialPortState state;

	serial::parity_t parity;
	serial::bytesize_t databits;
	serial::stopbits_t stopbits;
	serial_handle_sp_t ser;

	bool IsOpen();

	bool CloseSerial();

	bool OpenSerial();
};

struct BinBurnPlan {
	bool enable;
	char name[128];
	char fileName[128];
	char loadAddressText[128];
	uint32_t loadAddress;
	uint32_t fileSize;
	uint32_t color;
};

class IngBurner {
public:
	IngBurner() { 
		burnPlans.resize(BIN_BURN_PLAN_MAX_SIZE);
		for (uint32_t i = 0; i < BIN_BURN_PLAN_MAX_SIZE; ++i) {
			burnPlans[i].color = burn_plan_color[i];
		}
		serialPorts.resize(SERIAL_PORT_MAX_NUM);
		for (uint32_t i = 0; i < SERIAL_PORT_MAX_NUM; ++i) {
			strcpy(serialPorts[i].portName, ("COM" + std::to_string(i + 3)).c_str());
		}
		//memset(&options, 0, sizeof(Options));

		LoadFromini(default_config_file);
	}
	~IngBurner() {}

	void LoadFromini(std::filesystem::path ini_path)
	{
		INIReader reader(ini_path.u8string());

		family = reader.Get("main", "family", "ing918");

		options.download			= reader.GetBoolean("options", "download", true);
		options.verify				= reader.GetBoolean("options", "verify", false);
		options.redownload			= reader.GetBoolean("options", "redownload", true);
		options.entry				= reader.GetInteger("options", "entry", 0);
		options.set_entry			= reader.GetBoolean("options", "set_entry", true);
		options.launch				= reader.GetBoolean("options", "launch", true);
		options.batch				= reader.GetBoolean("options", "batch", false);
		options.batch_current		= reader.GetInteger("options", "batch.current", 0);
		options.batch_limit			= reader.GetInteger("options", "batch.limit", 0);
		options.protection_enabled	= reader.GetBoolean("options", "protection.enabled", false);
		options.protection_unlock	= reader.GetBoolean("options", "protection.unlock", false);
		options.resetReservedFlash  = reader.GetBoolean("options", "ResetReservedFlash", false);
		options.UseScript			= reader.GetBoolean("options", "UseScript", false);
		strcpy(options.script, reader.Get("options", "script", "").c_str());


		for (uint32_t i = 0; i < SERIAL_PORT_MAX_NUM; ++i) {
			auto& serialPort = serialPorts[i];
			std::string section = "uart-" + std::to_string(i);

			strcpy(serialPort.portName, reader.Get(section, "Port", "COM0").c_str());
			serialPort.baud = reader.GetInteger(section, "Baud", 115200);
			serialPort.parity = parity_c2v(reader.Get(section, "Parity", "none"));
			serialPort.databits = (serial::bytesize_t)reader.GetInteger(section, "DataBits", 8);
			serialPort.stopbits = (serial::stopbits_t)reader.GetInteger(section, "StopBits", 1);
			serialPort.state = SERIAL_PORT_UNUSED;
			strcpy(serialPort.baudText, std::to_string(serialPort.baud).c_str());
		}

		for (uint32_t i = 0; i < BIN_BURN_PLAN_MAX_SIZE; ++i) {
			auto& plan = burnPlans[i];
			std::string section = "bin-" + std::to_string(i);

			strcpy(plan.name, reader.Get(section, "Name", "Burn Bin #0").c_str());
			strcpy(plan.fileName, reader.Get(section, "FileName", "").c_str());
			plan.enable = reader.GetBoolean(section, "Checked", false);
			strcpy(plan.loadAddressText, reader.Get(section, "Address", "0").c_str());
		}
	}

	void SaveConfig()
	{
		std::ofstream out;
		out.open(default_config_file, std::ios::out);

		out << "[options]" << std::endl;
		out << "download=" << options.download << std::endl;
		out << "verify=" << options.verify << std::endl;
		out << "redownload=" << options.redownload << std::endl;
		out << "entry=" << options.entry << std::endl;
		out << "set-entry=" << options.set_entry << std::endl;
		out << "launch=" << options.launch << std::endl;
		out << "batch=" << options.batch << std::endl;
		out << "batch.current=" << options.batch_current << std::endl;
		out << "batch.limit=" << options.batch_limit << std::endl;
		out << "protection.enabled=" << options.protection_enabled << std::endl;
		out << "protection.unlock=" << options.protection_unlock << std::endl;
		out << "UseScript=" << options.UseScript << std::endl;
		out << "script=" << options.script << std::endl;
		out << "ResetReservedFlash=" << options.resetReservedFlash << std::endl;
		out << std::endl;

		out << "[main]" << std::endl;
		out << "family=" << family << std::endl;
		out << std::endl;

		for (uint32_t i = 0; i < SERIAL_PORT_MAX_NUM; ++i) {
			auto& serialPort = serialPorts[i];
			out << "[uart-" << i << "]" << std::endl;

			out << "Port=" << serialPort.portName << std::endl;
			out << "Baud=" << serialPort.baud << std::endl;
			out << "Parity=" << parity_v2c(serialPort.parity) << std::endl;
			out << "DataBits=" << serialPort.databits << std::endl;
			out << "StopBits=" << serialPort.stopbits << std::endl;
			out << std::endl;
		}

		for (uint32_t i = 0; i < BIN_BURN_PLAN_MAX_SIZE; ++i) {
			auto& plan = burnPlans[i];
			out << "[bin-" << i << "]" << std::endl;
			out << "Name=Burn Bin #" << (i + 1) << std::endl;
			out << "Checked=" << plan.enable << std::endl;
			out << "FileName=" << plan.fileName << std::endl;
			out << "Address=" << plan.loadAddressText << std::endl;
			out << std::endl;
		}

		out.close();
	}

	void SetWorkDir(std::filesystem::path dir) 
	{
		this->work_dir = dir;
	}

	std::filesystem::path GetWorkDir()
	{
		return this->work_dir;
	}

public:
	std::vector<BinBurnPlan> burnPlans;
	std::vector<SerialPort> serialPorts;

	Options options;

	std::string family;
	std::filesystem::path work_dir;
private:

};