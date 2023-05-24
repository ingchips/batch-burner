#include "burn.h"

#include "util/utils.h"
#include "Python.h"
#include <math.h>

extern BurnContext* burnContext;
extern ImLogger* logger;


std::string BurnContext::getState()
{
	switch (this->state)
	{
	case STATE_RUNNING:
		return "Running...";
	case STATE_ALL_SUCCESS:
		return "All success";
	case STATE_PARTIAL_FAILURE:
		return "Partial failure";
	default:
		return "";
	}
}

std::string PortInfo::getState()
{
	switch (this->state)
	{
	case STATE_WAIT_HANDSHAKE:
		return "waiting handshake...";
	case STATE_CHECK_PROTECTION_STATE:
		return "check protection state...";
	case STATE_SET_BAUD:
		return "set baud...";
	case STATE_BURNING:
		return "burning...";
	case STATE_SET_ENTRY:
		return "set entry...";
	case STATE_SET_PROTECTION:
		return "set protection...";
	case STATE_LAUNCH:
		return "launch app...";
	case STATE_COMPLETE:
		return "complete.";
	default:
		return "";
	}
}

std::string PortInfo::getResult()
{
	std::string r;
	switch (result) {
	case burn::RET_CODE_SUCCESS:
		r = "burn success";
		break;
	case burn::RET_CODE_CLOSE:
		r = "closed";
		break;
	case burn::RET_CODE_WAIT_HANDSHAKE_TIMEOUT:
		r = "wait handshake timeout";
		break;
	case burn::RET_CODE_FLASH_LOCKED:
		r = "flash is locked";
		break;
	case burn::RET_CODE_SCRIPT_ABORT:
		r = "script abort";
		break;
	case burn::RET_CODE_BATCH_COUTNER_LIMIT:
		r = "counter has been maximum";
		break;
	case burn::RET_CODE_FAIL_TO_SET_BAUD:
		r = "failed to set baud rate";
		break;
	case burn::RET_CODE_FAIL_TO_SET_FLASH_LOCK:
		r = "failed to set flash lock";
		break;
	case burn::RET_CODE_FAIL_TO_SET_ENTRY:
		r = "failed to set entry";
		break;
	case burn::RET_CODE_FAIL_TO_BURN_DATA:
		r = "burn failed";
		break;
	default:
		break;
	}
	return r;
}

bool PortInfo::isOver()
{
	return future->wait_for(std::chrono::microseconds(1)) != std::future_status::timeout;
}

bool BurnContext::allOver()
{
	for (auto& port : ports)
		if (port.future->wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout)
			return false;
	return true;
}

bool BurnContext::allSuccess()
{
	for (uint32_t i = 0; i < ports.size(); ++i) {
		if (ports[i].result != burn::RET_CODE_SUCCESS)
			return false;
	}
	return true;
}

uint16_t burn::calc_crc_16(uint8_t* PData, uint32_t size)
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

void burn::append_32_little(uint32_t v, uint8_t* outbuf)
{
	outbuf[0] = v >> 0;
	outbuf[1] = v >> 8;
	outbuf[2] = v >> 16;
	outbuf[3] = v >> 24;
}
void burn::append_16_little(uint16_t v, uint8_t* outbuf)
{
	outbuf[0] = v >> 0;
	outbuf[1] = v >> 8;
}
void burn::append_8_little(uint16_t v, uint8_t* outbuf)
{
	outbuf[0] = v >> 0;
}
void burn::append_str(const char* str, uint8_t* outbuf)
	{
		memcpy(outbuf, str, strlen(str));
	}

bool burn::wait_handshaking3(::serial::Serial* ser, int timeout, std::string hello, PortInfo& portInfo)
{
	long long start = ::utils::get_current_system_time_s();

	uint8_t acc[1024] = { 0 };
	size_t index = 0;
	size_t hello_len = hello.size();

	while (true) {
		size_t i = ser->read(acc + index, 1);
		index += i;
		if (index >= 1024)
			index = 0;

		if (index >= hello_len) {
			if (memcmp(hello.data(), (acc + index - hello_len), hello_len) == 0) {
				return true;
			}
		}
		if (::utils::get_current_system_time_s() - start > timeout) {
			return false;
		}

		if (portInfo.go)
			return true;

		if (burnContext->close)
			return false;
	}
	return false;
}

bool burn::OnStartBin(int batchCounter, int binIndex, std::vector<uint8_t>& data, std::vector<uint8_t>& out_data)
{
	auto engine = engine::getInstance();

	bool result = false;

	// Attaching a PythonThreadState to C thread, then hold the GIL
	PyGILState_STATE gstate = PyGILState_Ensure();	

	try
	{
		result = engine->OnStartBin(batchCounter, binIndex, data, out_data);
	}
	catch (std::exception e)
	{
		logger->AddLog("%s\r\n", e.what());
	}

	// release GIL
	PyGILState_Release(gstate);

	return result;
}



std::string burn::ing916::exec_cmd(serial::Serial* ser, uint8_t* cmd, uint32_t size)
{
	size = (uint32_t)ser->write(cmd, (size_t)size);
	return ser->read(RSP_LEN);
}
std::string burn::ing916::exec_cmd(serial::Serial* ser, std::string cmd)
{
	ser->write(cmd);
	return ser->read(RSP_LEN);
}


bool burn::ing916::is_locked(serial::Serial* ser)
{
	std::string rsp = exec_cmd(ser, (uint8_t*)CMD_QLOCKSTATE, 7);

	if (rsp == STATUS_LOCKED)
		return true;
	else if (rsp == STATUS_UNLOCKED)
		return false;
	else
		throw std::exception(("bad response:" + rsp + "\n").c_str());
	return false;
}
bool burn::ing916::unlock(serial::Serial* ser)
{
	return exec_cmd(ser, (uint8_t*)CMD_UNLOCK, 7) == ACK;
}
bool burn::ing916::lock(serial::Serial* ser)
{
	return exec_cmd(ser, (uint8_t*)CMD_LOCK, 7) == ACK;
}

bool burn::ing916::set_baud(serial::Serial* ser, int baud)
{
	uint8_t buf[11] = { 0 };
	append_str(CMD_SET_BAUD, buf + 0);
	append_32_little(baud, buf + 7);
	return exec_cmd(ser, buf, 11) == ACK;
}

bool burn::ing916::direct_jump(serial::Serial* ser, int entry_addr)
{
	uint8_t buf[11] = { 0 };
	append_str(CMD_JUMPD, buf + 0);
	append_32_little(entry_addr, buf + 7);
	return exec_cmd(ser, buf, 11) == ACK;
}
void burn::ing916::launch_app(serial::Serial* ser)
{
	ser->write((uint8_t*)CMD_LAUNCH, 7);
}


bool burn::ing916::erase_sector(serial::Serial* ser, int addr)
{
	uint8_t buf[11] = { 0 };
	append_str(ERASE_SECTOR, buf + 0);
	append_32_little(addr, buf + 7);
	return exec_cmd(ser, buf, 11) == ACK;
}
//C:/Users/leosh/AppData/Roaming/Local/Program/ING_SDK/sdk/examples/transmission_zy/flash_download.ini

bool burn::ing916::send_page(serial::Serial* ser, int addr, uint8_t* data, uint32_t size)
{
	uint8_t buf[12] = { 0 };
	if (addr < RAM_BASE_ADDR)
		append_str(SEND_PAGE, buf + 0);
	else
		append_str(SEND_RAM_DATA, buf + 0);
	append_32_little(addr, buf + 7);
	append_8_little(size - 1, buf + 11);

	std::string rsp = exec_cmd(ser, buf, 12);
	if (rsp != ACK)
		return false;

	ser->write(data, size);
	uint16_t crc = calc_crc_16(data, size);
	append_16_little(crc, buf + 0);
	ser->write(buf, 2);
	rsp = ser->read(RSP_LEN);
	return rsp == ACK;
}

bool burn::ing916::send_sector(serial::Serial* ser, int addr, uint8_t* data, uint32_t size)
{
	if (addr < RAM_BASE_ADDR)
		if (!erase_sector(ser, addr))
			return false;

	uint32_t sub_offset = 0;

	while (sub_offset < size) {
		uint32_t seg = size - sub_offset;
		if (seg > PAGE_SIZE)
			seg = PAGE_SIZE;

		if (!send_page(ser, addr + sub_offset, data + sub_offset, seg))
			return false;

		sub_offset += seg;
	}
	return true;
}


bool burn::ing916::send_file(serial::Serial* ser, int addr, uint8_t* data, uint32_t size, PortInfo& portInfo)
{
	while (size > (uint32_t)portInfo.current_position) {
		uint32_t seg = size - portInfo.current_position;
		if (seg > SECTOR_SIZE)
			seg = SECTOR_SIZE;

		if (!send_sector(ser, addr + portInfo.current_position, data + portInfo.current_position, seg))
			return false;

		portInfo.current_position += seg;
		portInfo.global_position += seg;
	}
	return true;
}

std::string burn::ing918::exec_cmd(serial::Serial* ser, uint8_t* cmd, uint32_t size)
{
	size = (uint32_t)ser->write(cmd, (size_t)size);
	return ser->read(RSP_LEN);
}
std::string burn::ing918::exec_cmd(serial::Serial* ser, std::string cmd)
{
	ser->write(cmd);
	return ser->read(RSP_LEN);
}

bool burn::ing918::is_locked(serial::Serial* ser)
{
	std::string rsp = exec_cmd(ser, (uint8_t*)CMD_QLOCKSTATE, 7);

	if (rsp == STATUS_LOCKED)
		return true;
	else if (rsp == STATUS_UNLOCKED)
		return false;
	else
		throw std::exception(("bad response:" + rsp).c_str());
}
bool burn::ing918::unlock(serial::Serial* ser)
{
	return exec_cmd(ser, (uint8_t*)CMD_UNLOCK, 7) == ACK;
}
bool burn::ing918::lock(serial::Serial* ser)
{
	return exec_cmd(ser, (uint8_t*)CMD_LOCK, 7) == ACK;
}

bool burn::ing918::set_baud(serial::Serial* ser, int baud)
{
	uint8_t buf[11] = { 0 };
	append_str(CMD_SET_BAUD, buf + 0);
	append_32_little(baud, buf + 7);
	return exec_cmd(ser, buf, 11) == ACK;
}

bool burn::ing918::direct_jump(serial::Serial* ser, int entry_addr)
{
	uint8_t buf[11] = { 0 };
	append_str(CMD_JUMPD, buf + 0);
	append_32_little(entry_addr, buf + 7);
	return exec_cmd(ser, buf, 11) == ACK;
}
bool burn::ing918::set_entry(serial::Serial* ser, int entry_addr)
{
	uint8_t buf[11] = { 0 };
	append_str(SET_ENTRY, buf + 0);
	append_32_little(entry_addr, buf + 7);
	return exec_cmd(ser, buf, 11) == ACK;
}
void burn::ing918::launch_app(serial::Serial* ser)
{
	ser->write((uint8_t*)CMD_LAUNCH, 7);
}

bool burn::ing918::send_page(serial::Serial* ser, int addr, uint8_t* data, uint32_t offset, uint32_t size)
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
bool burn::ing918::send_file(serial::Serial* ser, int addr, uint8_t* data, uint32_t size, PortInfo& portInfo)
{
	while (size > (uint32_t)portInfo.current_position) {
		uint32_t seg = size - portInfo.current_position;
		if (seg > PAGE_SIZE)
			seg = PAGE_SIZE;

		if (!send_page(ser, addr, data, portInfo.current_position, seg))
			return false;

		portInfo.current_position += seg;
		portInfo.global_position += seg;
	}
	return true;
}

bool burn::ing918::BurnRun0(BurnContext* ctx, int index)
{
	auto& port = ctx->ports[index];
	port.state = STATE_WAIT_HANDSHAKE;
	port.result = 0;
	port.current_bin_index = 0;
	port.current_limit = (int)ctx->bins[port.current_bin_index].bin_data.size();
	port.current_position = 0;
	port.global_limit = ctx->bins_total_size;
	port.global_position = 0;


	serial::Serial* ser = port.ser.get();
	ser->setBaudrate(DEF_BAUD);

	if (ser->isOpen())
		ser->close();
	ser->open();

	if (ctx->burner->options.batch) {
		if (port.counter >= ctx->burner->options.batch_limit) {
			port.result = RET_CODE_BATCH_COUTNER_LIMIT;
			return false;
		}
	}

	if (!ser->isOpen()) {
		throw std::exception("The serial port is not open");
	}

	if (!port.go) {
		port.state = STATE_WAIT_HANDSHAKE;
		ctx->logger->AddLog("[%s]Wait for handshake\n", port.portName);
		if (!wait_handshaking3(ser, 3600, BOOT_HELLO, port)) {
			if (ctx->close) {
				ctx->logger->AddLog("[%s]close\n", port.portName);
				port.result = RET_CODE_CLOSE;
				return false;
			} else {
				ctx->logger->AddLog("[%s]handshaking timeout\n", port.portName);
				port.result = RET_CODE_WAIT_HANDSHAKE_TIMEOUT;
				return false;
			}
		}
		else {
			ctx->logger->AddLog("[%s]handshaking ok\n", port.portName);
			ser->flushInput();
		}
	}

	port.state = STATE_CHECK_PROTECTION_STATE;
	if (is_locked(ser)) {
		if (ctx->burner->options.protection_unlock)
			unlock(ser);
		else {
			ctx->logger->AddLog("[%s]flash locked\n", port.portName);
			port.result = RET_CODE_FLASH_LOCKED;
			return false;
		}
	}

	int uart_baud = port.baud;
	if (uart_baud != DEF_BAUD) {
		port.state = STATE_SET_BAUD;
		ctx->logger->AddLog("[%s]baud -> %d\n", port.portName, uart_baud);
		if (!set_baud(ser, uart_baud)) {
			port.result = RET_CODE_FAIL_TO_SET_BAUD;
			return false;
		}
		utils::my_sleep(100);
		ser->setBaudrate(uart_baud);
	}

	port.state = STATE_BURNING;
	while (port.current_bin_index < ctx->bins.size())
	{
		auto& binInfo = ctx->bins[port.current_bin_index];
		port.current_position = 0;
		port.current_limit = (int)binInfo.bin_data.size();

		ctx->logger->AddLog("[%s]downloading %s %d\n", port.portName, binInfo.bin_name, binInfo.load_address);

		std::vector<uint8_t>& realData = binInfo.bin_data;
		if (ctx->burner->options.UseScript) {
			std::vector<uint8_t> bytes;
			if (!OnStartBin(port.counter, port.current_bin_index, binInfo.bin_data, bytes)) {
				port.result = RET_CODE_SCRIPT_ABORT;
				return false;
			}
			realData = bytes;
		}

		if (send_file(ser, binInfo.load_address, realData.data(), (uint32_t)realData.size(), port))
			port.current_bin_index++;
		else {
			port.result = RET_CODE_FAIL_TO_BURN_DATA;
			return false;
		}
	}

	if (ctx->burner->options.set_entry) {
		port.state = STATE_SET_ENTRY;
		if (ctx->burner->options.entry >= RAM_BASE_ADDR) {
			direct_jump(ser, ctx->burner->options.entry);
			port.state = STATE_COMPLETE;
			port.result = RET_CODE_SUCCESS;
			return true;
		}
		else
			if (!set_entry(ser, ctx->burner->options.entry)) {
				port.result = RET_CODE_FAIL_TO_SET_ENTRY;
				return false;
			}
	}

	if (ctx->burner->options.protection_enabled) {
		port.state = STATE_SET_PROTECTION;
		if (!lock(ser)) {
			port.result = RET_CODE_FAIL_TO_SET_FLASH_LOCK;
			return false;
		}
	}

	port.state = STATE_LAUNCH;
	if (ctx->burner->options.launch)
		launch_app(ser);

	port.state = STATE_COMPLETE;
	port.result = RET_CODE_SUCCESS;
	return true;
}


bool burn::ing916::BurnRun0(BurnContext* ctx, int index)
{
	PortInfo& port = ctx->ports[index];
	port.state = STATE_WAIT_HANDSHAKE;
	port.result = 0;
	port.current_bin_index = 0;
	port.current_limit = (int)ctx->bins[port.current_bin_index].bin_data.size();
	port.current_position = 0;
	port.global_limit = ctx->bins_total_size;
	port.global_position = 0;

	auto& options = ctx->burner->options;
	auto& logger = ctx->logger;

	serial::Serial* ser = port.ser.get();
	ser->setBaudrate(DEF_BAUD);

	if (ser->isOpen())
		ser->close();
	ser->open();

	if (ctx->burner->options.batch) {
		if (port.counter >= ctx->burner->options.batch_limit) {
			port.result = RET_CODE_BATCH_COUTNER_LIMIT;
			return false;
		}
	}

	if (!ser->isOpen()) {
		throw std::exception("The serial port is not open");
	}

	if (!port.go) {
		port.state = STATE_WAIT_HANDSHAKE;
		ctx->logger->AddLog("[%s]Wait for handshake\n", port.portName);
		if (!wait_handshaking3(ser, 3600, BOOT_HELLO, port)) {
			if (ctx->close) {
				ctx->logger->AddLog("[%s]close\n", port.portName);
				port.result = RET_CODE_CLOSE;
				return false;
			}
			else {
				ctx->logger->AddLog("[%s]handshaking timeout\n", port.portName);
				port.result = RET_CODE_WAIT_HANDSHAKE_TIMEOUT;
				return false;
			}
		}
		else {
			ctx->logger->AddLog("[%s]handshaking ok\n", port.portName);
			ser->flushInput();
		}
	}

	port.state = STATE_CHECK_PROTECTION_STATE;
	if (is_locked(ser)) {
		if (options.protection_unlock)
			unlock(ser);
		else {
			logger->AddLog("flash locked\n");
			port.result = RET_CODE_FLASH_LOCKED;
			return false;
		}
	}

	if (options.resetReservedFlash) {
		erase_sector(ser, 0x2000000);
	}

	if (DEF_BAUD != port.baud) {
		port.state = STATE_SET_BAUD;
		if (!set_baud(ser, port.baud)) {
			port.result = RET_CODE_FAIL_TO_SET_BAUD;
			return false;
		}
		utils::my_sleep(1);
		ser->setBaudrate(port.baud);
	}

	port.state = STATE_BURNING;

	while (port.current_bin_index < ctx->bins.size())
	{
		auto& binInfo = ctx->bins[port.current_bin_index];
		port.current_position = 0;
		port.current_limit = (int)binInfo.bin_data.size();

		ctx->logger->AddLog("[%s]downloading %s %d\n", port.portName, binInfo.bin_name, binInfo.load_address);

		std::vector<uint8_t>& realData = binInfo.bin_data;
		if (ctx->burner->options.UseScript) {
			std::vector<uint8_t> bytes;
			if (!OnStartBin(port.counter, port.current_bin_index, binInfo.bin_data, bytes)) {
				port.result = RET_CODE_SCRIPT_ABORT;
				return false;
			}
			realData = bytes;
		}

		if (send_file(ser, binInfo.load_address, realData.data(), (uint32_t)realData.size(), port))
			port.current_bin_index++;
		else {
			port.result = RET_CODE_FAIL_TO_BURN_DATA;
			return false;
		}
	}


	if (ctx->burner->options.set_entry) {
		port.state = STATE_SET_ENTRY;
		if (ctx->burner->options.entry >= RAM_BASE_ADDR) {
			direct_jump(ser, ctx->burner->options.entry);
			port.state = STATE_COMPLETE;
			port.result = RET_CODE_SUCCESS;
			return true;
		}
		else;
	}

	if (ctx->burner->options.protection_enabled) {
		port.state = STATE_SET_PROTECTION;
		if (!lock(ser)) {
			port.result = RET_CODE_FAIL_TO_SET_FLASH_LOCK;
			return false;
		}
	}

	port.state = STATE_LAUNCH;
	if (ctx->burner->options.launch)
		launch_app(ser);

	port.state = STATE_COMPLETE;
	port.result = RET_CODE_SUCCESS;
	return true;

}