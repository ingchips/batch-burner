#include <iostream>
#include <windows.h>
#include <vector>
#include <filesystem>
#include "imgui_impl_win32.h"

#include "winbase.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "ImFileDialog.h"
#include "ImConsole.h"
#include "TextEditor.h"

#include "util/utils.h"
#include "util/thread_pool.h"
#include "ingBurner.h"
#include "burn.h"

#include "Python.h"

#define DEFAULT_FONT "c:\\Windows\\Fonts\\msyh.ttc"
#define FILE_SELELCTOR_KEY_OPEN_INI_CONFIG "Open ini config"
#define BURN_PANEL_TITLE "burn panel"
#define SCRIPT_EDIT_TITLE "Edit Script"
#define IMIDTEXT(name, i) ((std::string(name) + std::to_string(i)).c_str())

static bool show_root_window = true;

static bool opt_fullscreen = true;
static bool opt_showdemowindow = false;
static bool opt_padding = false;
static bool opt_showlogwindow = true;
static bool opt_showportwindow = true;
static bool opt_showbinwindow = true;
static bool opt_showconfigwindow = true;

ImVec4 clear_color = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
IngBurner* burner;
ImLogger* logger;
BurnContext* burnContext;
ThreadPool* pool;

std::filesystem::path get_file_path(std::string filename)
{
	if (std::filesystem::path(filename).is_absolute())
		return filename;
	else
		return burner->GetWorkDir().u8string().append("/").append(filename);
}

extern void* CreateTexture(uint8_t* data, int w, int h, char fmt);
extern void DeleteTexture(void* tex);

static std::filesystem::path default_script_file = std::filesystem::current_path().concat("\\intercept.py");
static TextEditor editor;
static char fileToEdit[256] = { 0 };

static void init_text_editor()
{
	auto lang = TextEditor::LanguageDefinition::CPlusPlus();
	editor.SetLanguageDefinition(lang);

	editor.SetPalette(TextEditor::GetLightPalette());

	strcpy(fileToEdit, default_script_file.u8string().c_str());
	{
		std::ifstream t(fileToEdit);
		if (t.good())
		{
			std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
			editor.SetText(str);
		}
	}
}

static bool main_init(int argc, char* argv[])
{


	// Enable Dock
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	
	// Theme
	ImGui::StyleColorsLight();

	// File dialog adapter
	ifd::FileDialog::Instance().CreateTexture = [](uint8_t* data, int w, int h, char fmt) -> void* {
		return CreateTexture(data, w, h, fmt);
	};
	ifd::FileDialog::Instance().DeleteTexture = [](void* tex) {
		DeleteTexture(tex);
	};

	// Font
	io.Fonts->AddFontFromFileTTF(DEFAULT_FONT, 18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

	burner = new IngBurner();
	logger = new ImLogger();

	pool = new ThreadPool(25);

	init_text_editor();

	// Init CPython
	if (argc > 1)
	{
		engine::setPythonHome(argv[1]);
		auto engine = engine::getInstance();
	}
	else
	{
		auto engine = engine::getInstance();
	}
	

    return true;
}

static void main_shutdown(void)
{
	// Release resource
}

static void ImGuiDCXAxisAlign(float v)
{
	ImGui::SetCursorPos(ImVec2(v, ImGui::GetCursorPos().y));
}

static void ImGuiDCYMargin(float v)
{
	ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x, ImGui::GetCursorPos().y + v));
}

static void OpenConfigFile()
{
	ifd::FileDialog::Instance().Open(FILE_SELELCTOR_KEY_OPEN_INI_CONFIG, "Choose .ini", ".*", false);
}

static void ShowOpenConfigFileDialog()
{
	if (ifd::FileDialog::Instance().IsDone(FILE_SELELCTOR_KEY_OPEN_INI_CONFIG)) {
		if (ifd::FileDialog::Instance().HasResult()) {
			const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
			if (res.size() > 0) {
				std::filesystem::path path = res[0];

				if (path.extension().u8string() == ".ini") {
					burner->SetWorkDir(path.parent_path());

					burner->LoadFromini(path);

					logger->AddLog("open ini\n");
					logger->AddLog("set work dir:");
					logger->AddLog(burner->GetWorkDir().u8string().c_str());
					logger->AddLog("\n");
				} else {
					logger->AddLog("ini invalid\n");
				}
			}
		}
		ifd::FileDialog::Instance().Close();
	}
}

static void ShowRootWindowMenu()
{
	if (ImGui::BeginMenuBar()) {

		if (ImGui::BeginMenu("File")) {

			if (ImGui::MenuItem("Open ini")) {
				OpenConfigFile();
			}

			if (ImGui::MenuItem("Save", "Ctrl + S")) {
				burner->SaveConfig();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("Log Window", NULL, &opt_showlogwindow);
			ImGui::MenuItem("Bin Window", NULL, &opt_showbinwindow);
			ImGui::MenuItem("Port Window", NULL, &opt_showportwindow);
			ImGui::MenuItem("Config Window", NULL, &opt_showconfigwindow);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Option")) {
			//ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
			//ImGui::Separator();

			ImGui::MenuItem("Demo Window", NULL, &opt_showdemowindow);


			if (ImGui::MenuItem("Theme Light")) {
				ImGui::StyleColorsLight();
			}
			if (ImGui::MenuItem("Theme Dark")) {
				ImGui::StyleColorsDark();
			}
			if (ImGui::MenuItem("Theme Classic")) {
				ImGui::StyleColorsClassic();
			}

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ShowOpenConfigFileDialog();
}

static void ShowComList()
{

}

static void ShowFlash() 
{
	ImGui::Begin("Flash");
	ImGuiWindow* window = ImGui::GetCurrentWindow();


	ImGui::End();
}

static bool IsValidSerialPort(char* portName) 
{
	return true;
}

static void ShowLog(bool* p_opt_showlogwindow)
{
	logger->Draw("logger", p_opt_showlogwindow);
}

//=============================================================================


static bool BurnRun0(BurnContext* ctx, int index)
{
	auto& port = ctx->ports[index];
	try
	{
		if (burner->family == "ing918")
			return burn::ing918::BurnRun0(ctx, index);
		else if (burner->family == "ing916")
			return burn::ing916::BurnRun0(ctx, index);
		else
			return false;
	}
	catch (std::exception e)
	{
		logger->AddLog("%s\r\n", e.what());
	}
	return false;
}

static bool all_over(std::vector<std::future<bool>>& futures)
{
	for (auto& future : futures)
		if (future.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout)
			return false;
	return true;
}


static void BurnContextResetPortContext();
static void BurnContextStartNormal();


static void BurnRunForce(BurnContext* ctx, int index)
{
	ctx->state = STATE_RUNNING;

	auto& port = ctx->ports[index];
	port.go = true;

	port.future = std::make_shared<std::future<bool>>(pool->enqueue(BurnRun0, ctx, index));

	bool r = port.future.get();
	if (r == false || port.result != burn::RET_CODE_SUCCESS) {
		logger->AddLog("[Thread]Force burn Failed\n");
		return;
	} else {
		logger->AddLog("[Thread]Force burn Success\n");
	}
	
	if (ctx->allSuccess()) {
		ctx->state = STATE_ALL_SUCCESS;
		logger->AddLog("[Thread]All Success\n");
		utils::my_sleep(1000);
		BurnContextResetPortContext();
		BurnContextStartNormal();
	}
}

static void BurnRun(BurnContext* ctx)
{
	ctx->state = STATE_RUNNING;

	for (uint32_t i = 0; i < ctx->ports.size(); ++i) {
		auto& port = ctx->ports[i];
		port.future = std::make_shared<std::future<bool>>(pool->enqueue(BurnRun0, ctx, i));
	}

	while (true) {
		if (ctx->allOver())
			break;
		utils::my_sleep(100);
	}

	if (ctx->allSuccess())
	{
		ctx->state = STATE_ALL_SUCCESS;
		logger->AddLog("[Thread]All Success\n");

		burner->options.batch_current += (int)ctx->ports.size();

		utils::my_sleep(1000);

		BurnContextResetPortContext();
		BurnContextStartNormal();
	}
	else
	{
		ctx->state = STATE_PARTIAL_FAILURE;

		if (ctx->close)
			logger->AddLog("[Thread]Closed\n");
		else
			logger->AddLog("[Thread]Partial faliure\n");
	}
}

static void BurnContextInitBinData()
{
	// Prepare bin data
	burnContext->bins.clear();
	for (uint32_t i = 0; i < BIN_BURN_PLAN_MAX_SIZE; ++i) {
		auto& plan = burner->burnPlans[i];

		if (!plan.enable)
			continue;

		auto path = get_file_path(plan.fileName);

		if (!std::filesystem::is_regular_file(path))
			throw std::exception("File not exists");

		uint32_t filesize = static_cast<uint32_t>(std::filesystem::file_size(path));

		BinInfo binInfo;
		binInfo.bin_data.resize(filesize);
		binInfo.load_address = plan.loadAddress;

		burnContext->bins_total_size += filesize;

		strcpy(binInfo.bin_name, path.filename().u8string().c_str());
		utils::readFileData(path, binInfo.bin_data.data());
		burnContext->bins.push_back(binInfo);
	}
}

static void BurnContextResetPortContext()
{
	int counter = burner->options.batch_current;

	for (uint32_t i = 0; i < burnContext->ports.size(); ++i) {
		auto& portInfo = burnContext->ports[i];

		portInfo.state = STATE_WAIT_HANDSHAKE;
		portInfo.go = false;
		portInfo.current_bin_index = 0;
		portInfo.current_limit = (int)burnContext->bins[portInfo.current_bin_index].bin_data.size();
		portInfo.current_position = 0;
		portInfo.global_limit = burnContext->bins_total_size;
		portInfo.global_position = 0;
		portInfo.counter = counter++;	// counter increment
	}
}
static void BurnContextInitPortContext()
{
	int counter = burner->options.batch_current;

	// Prepare port context
	burnContext->ports.clear();
	for (uint32_t i = 0; i < SERIAL_PORT_MAX_NUM; ++i) {
		auto& port = burner->serialPorts[i];

		if (!port.enable || !port.IsOpen())
			continue;

		PortInfo portInfo;
		strcpy(portInfo.portName, port.portName);
		portInfo.baud = port.baud;
		portInfo.ser = port.ser;
		portInfo.state = STATE_WAIT_HANDSHAKE;
		portInfo.go = false;
		portInfo.current_bin_index = 0;
		portInfo.current_limit = (int)burnContext->bins[portInfo.current_bin_index].bin_data.size();
		portInfo.current_position = 0;
		portInfo.global_limit = (int)burnContext->bins_total_size;
		portInfo.global_position = 0;
		portInfo.counter = counter++;
		portInfo.color = port_color[i];
		burnContext->ports.push_back(portInfo);
	}
}

static void BurnContextStartForce(int index)
{
	auto result = pool->enqueue(BurnRunForce, burnContext, index);
}
static void BurnContextStartNormal()
{
	if (burnContext->ports.size() > 0) {
		auto result = pool->enqueue(BurnRun, burnContext);
	}
	burnContext->state = STATE_RUNNING;
}

static void BurnContextInit()
{
	burnContext = new BurnContext();

	burnContext->bins_total_size = 0;
	burnContext->bins.clear();
	burnContext->ports.clear();
	burnContext->logger = logger;
	burnContext->burner = burner;
	burnContext->close = false;

	BurnContextInitBinData();

	BurnContextInitPortContext();

	BurnContextStartNormal();
}

static void BurnContextDestory()
{
	delete burnContext;
	burnContext = NULL;
}

static void PrepareBurnContext()
{
	if (burnContext != NULL) {
		BurnContextDestory();
		burnContext = NULL;
	}
	BurnContextInit();
}

//=============================================================================

static void ShowForceBurnButton(int index)
{
	auto& portInfo = burnContext->ports[index];

	if (portInfo.isOver() && portInfo.result != burn::RET_CODE_SUCCESS)
	{
		ImGui::SameLine();
		if (ImGui::Button(IMIDTEXT("Force##Force", index))) {
			BurnContextStartForce(index);
		}
	}
}

static void ShowBurnPanel_BurnPlanTable()
{
	static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("table1", 3, flags))
	{
		ImGui::TableSetupColumn("Bin File Name");
		ImGui::TableSetupColumn("Load Address");
		ImGui::TableSetupColumn("File Size");

		ImGui::TableHeadersRow();

		for (uint32_t i = 0; i < burner->burnPlans.size(); ++i)
		{
			auto& plan = burner->burnPlans[i];

			if (!plan.enable)
				continue;

			ImGui::TableNextRow();

			std::filesystem::path path(plan.fileName);


			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(path.filename().u8string().c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(utils::dec2hex(plan.loadAddress).c_str());
			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted((std::to_string(plan.fileSize) + "B").c_str());
		}
		ImGui::EndTable();
	}
}

static void ShowBurnPanel_BurnComTable()
{
	if (burner->options.batch) {
		ImGui::Text("Enable Counter %d/%d", burner->options.batch_current, burner->options.batch_limit);
	}
	ImGui::Text("%s", burnContext->getState().c_str());

	if (burnContext->state == STATE_PARTIAL_FAILURE)
	{
		if (ImGui::Button("Restart"))
		{
			BurnContextStartNormal();
		}
	}


	const float progress_global_size = 500;
	static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("table2", 6, flags))
	{
		ImGui::TableSetupColumn("COM");
		ImGui::TableSetupColumn("Counter");
		ImGui::TableSetupColumn("Action");
		ImGui::TableSetupColumn("Progress");
		ImGui::TableSetupColumn("State");
		ImGui::TableSetupColumn("Result");

		ImGui::TableHeadersRow();

		for (uint32_t i = 0; i < burnContext->ports.size(); ++i) 
		{
			auto& port = burnContext->ports[i];

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text(port.portName);

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", port.counter);

			ImGui::TableSetColumnIndex(2);

			ImGui::TableSetColumnIndex(3);
			//if (port.current_bin_index < burnContext->bins.size()) {
			//	ImGui::Text(burnContext->bins[port.current_bin_index].bin_name);
			//	ImGui::SameLine();
			//	ImGuiDCXAxisAlign(progress_lm);
			//	float progress_current = (float)port.current_position / port.current_limit;
			//	float progress_current_saturated = std::clamp(progress_current, 0.0f, 1.0f);
			//	char buf_current[32];
			//	sprintf(buf_current, "%d/%d", (int)(progress_current_saturated * port.current_limit), port.current_limit);
			//	ImGui::ProgressBar(progress_current, ImVec2(progress_current_size, 0.f), buf_current);
			//}
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, port.color);
			float progress_global = (float)port.global_position / port.global_limit;
			float progress_global_saturated = std::clamp(progress_global, 0.0f, 1.0f);
			char buf_global[32];
			sprintf(buf_global, "%d/%d", (int)(progress_global_saturated * port.global_limit), port.global_limit);
			ImGui::ProgressBar(progress_global, ImVec2(progress_global_size, 0.f), buf_global);
			ImGui::PopStyleColor();

			ImGui::TableSetColumnIndex(4);
			ImGui::Text(port.getState().c_str());

			ImGui::TableSetColumnIndex(5);
			ImGui::Text(port.getResult().c_str());
		}

		ImGui::EndTable();
	}
}

static void ShowBurnPanel()
{
	if (ImGui::BeginPopupModal(BURN_PANEL_TITLE, NULL, ImGuiWindowFlags_None))
	{
		ShowBurnPanel_BurnPlanTable();


		ShowBurnPanel_BurnComTable();

		if (ImGui::Button("Close"))
		{
			burnContext->close = true;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

static void ShowPortConfig(bool* p_opt_showportwindow)
{
	bool show_alert_port_open_failed = false;
	bool show_alert_please_select_port = false;
	bool show_alert_please_select_firmware = false;

	ImGui::Begin("Port", p_opt_showportwindow);
	ImGuiWindow* window = ImGui::GetCurrentWindow();

	if (ImGui::Button("Start"))
	{
		PrepareBurnContext();

		if (burnContext->bins.size() <= 0)
			show_alert_please_select_firmware = true;
		if (burnContext->ports.size() <= 0)
			show_alert_please_select_port = true;
		else
			ImGui::OpenPopup(BURN_PANEL_TITLE);
	}

	ShowBurnPanel();

	const float cw = 5;

	static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("port_config_table", 5, flags))
	{
		ImGui::TableSetupColumn("Check");
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Baud");
		ImGui::TableSetupColumn("Action");
		ImGui::TableSetupColumn("State");

		ImGui::TableHeadersRow();

		for (uint32_t i = 0; i < SERIAL_PORT_MAX_NUM; ++i) {


			SerialPort& port = burner->serialPorts[i];

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			float item_top_y = window->DC.CursorPos.y;
			float item_top_x = window->DC.CursorPos.x;
			ImGuiDCXAxisAlign(cw + 10);
			if (!port.IsOpen()) ImGui::BeginDisabled();
			ImGui::Checkbox(IMIDTEXT("##Enable", i), &port.enable);
			if (!port.IsOpen()) ImGui::EndDisabled();

			ImGui::TableSetColumnIndex(1);
			ImGui::InputTextEx(IMIDTEXT("##SerialPort", i), "", port.portName, sizeof(port.portName), ImVec2(200.0f, 0), ImGuiInputTextFlags_None);
			

			ImGui::TableSetColumnIndex(2);
			ImGui::InputTextEx(IMIDTEXT("##BaudRate", i), "", port.baudText, sizeof(port.baudText), ImVec2(200.0f, 0), ImGuiInputTextFlags_CharsDecimal);
			
			if (strlen(port.baudText) > 0) {
				port.baud = std::stoi(port.baudText);
			}

			ImGui::TableSetColumnIndex(3);
			ImGui::PushStyleColor(ImGuiCol_Button, port_color[i]);
			if (port.IsOpen()) {
				if (ImGui::Button(IMIDTEXT("Close##Close", i)))
				{
					port.CloseSerial();
					port.enable = false;
				}

				ImGui::TableSetColumnIndex(4);
				ImGui::Text("Serial port Ready");
			}
			else 
			{
				if (ImGui::Button(IMIDTEXT("Open##Open", i)))
				{
					bool isOpen = port.OpenSerial();
					show_alert_port_open_failed = !isOpen;
					if (isOpen) port.enable = true;
				}

				ImGui::TableSetColumnIndex(4);
				ImGui::Text("");
			}
			ImGui::PopStyleColor();

			float item_bottom_y = window->DC.CursorPos.y;

			window->DrawList->AddRectFilled(ImVec2(item_top_x, item_top_y), ImVec2(item_top_x + cw, item_bottom_y), port_color[i]);
		}
		ImGui::EndTable();
	}

	utils::Alert(show_alert_port_open_failed, "Message##m1", "The current serial port number cannot be opened, please check and reopen.");
	utils::Alert(show_alert_please_select_port, "Message##m2", "Please select at least one.");
	utils::Alert(show_alert_please_select_firmware, "Message##m3", "Please select firmware.");


	ImGui::End();
}

static void ShowScriptEditor()
{
	auto cpos = editor.GetCursorPosition();

	ImGui::PushStyleColor(ImGuiCol_PopupBg, 0xFFEFEFEF);
	if (ImGui::BeginPopupModal(SCRIPT_EDIT_TITLE, NULL, ImGuiWindowFlags_MenuBar))
	{
		bool close = false;
		bool save = false;

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Save", "Ctrl + S"))
					save = true;
				if (ImGui::MenuItem("Quit", "Esc"))
					close = true;
				if (ImGui::MenuItem("Save & Quit", "Ctrl + ALT + S"))
					save = close = true;
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				bool ro = editor.IsReadOnly();
				if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
					editor.SetReadOnly(ro);
				ImGui::Separator();

				if (ImGui::MenuItem("Undo", "ALT-Backspace", nullptr, !ro && editor.CanUndo()))
					editor.Undo();
				if (ImGui::MenuItem("Redo", "Ctrl-Y", nullptr, !ro && editor.CanRedo()))
					editor.Redo();

				ImGui::Separator();

				if (ImGui::MenuItem("Copy", "Ctrl-C", nullptr, editor.HasSelection()))
					editor.Copy();
				if (ImGui::MenuItem("Cut", "Ctrl-X", nullptr, !ro && editor.HasSelection()))
					editor.Cut();
				if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && editor.HasSelection()))
					editor.Delete();
				if (ImGui::MenuItem("Paste", "Ctrl-V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
					editor.Paste();

				ImGui::Separator();

				if (ImGui::MenuItem("Select all", nullptr, nullptr))
					editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(editor.GetTotalLines(), 0));

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Dark palette"))
					editor.SetPalette(TextEditor::GetDarkPalette());
				if (ImGui::MenuItem("Light palette"))
					editor.SetPalette(TextEditor::GetLightPalette());
				if (ImGui::MenuItem("Retro blue palette"))
					editor.SetPalette(TextEditor::GetRetroBluePalette());
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
			editor.IsOverwrite() ? "Ovr" : "Ins",
			editor.CanUndo() ? "*" : " ",
			"Python", fileToEdit);

		editor.Render("TextEditor");

		if (ImGui::IsKeyDown(ImGuiKey_Escape))
			close = true;
		if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiKey_S))
			save = true;
		if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiMod_Alt) && ImGui::IsKeyDown(ImGuiKey_S))
			save = close = true;

		if (save) {
			auto textToSave = editor.GetText();
			std::ofstream out;
			out.open(default_script_file, std::ios::out);

			out << textToSave;

			out.flush();
			out.close();
		}

		if (close)
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	ImGui::PopStyleColor();
}

static void ShowConfigPanel(bool* p_opt_showconfigwindow)
{
	float second_column_align = 200.0f;

	ImGui::Begin("Config", p_opt_showconfigwindow);

	ImGui::Text("Options");

	if (ImGui::CollapsingHeader("Main"))
	{
		const char* items[] = { "ing918", "ing916"};
		static int item_current_idx = 0; // Here we store our selection data as an index.

		for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			if (burner->family == items[n]) {
				item_current_idx = n;
				break;
			}

		const char* combo_preview_value = items[item_current_idx];  // Pass in the preview value visible before opening the combo (it could be anything)
		if (ImGui::BeginCombo("Family", combo_preview_value, 0))
		{
			for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			{
				const bool is_selected = (item_current_idx == n);
				if (ImGui::Selectable(items[n], is_selected)) {
					item_current_idx = n;
					logger->AddLog("select:%s\r\n", items[n]);
					burner->family = items[n];
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	if (ImGui::CollapsingHeader("Protection"))
	{
		ImGui::Text("Enable");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(second_column_align);
		ImGui::Checkbox("##protection_enable", &burner->options.protection_enabled);

		ImGui::Text("Unlock");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(second_column_align);
		ImGui::Checkbox("##protection_unlock", &burner->options.protection_unlock);
	}

	if (ImGui::CollapsingHeader("Script"))
	{
		ImGui::Text("Enable");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(second_column_align);
		ImGui::Checkbox("##script_enable", &burner->options.UseScript);

		ImGui::Text("Script");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(second_column_align);
		if (ImGui::Button("Edit")) {
			ImGui::OpenPopup(SCRIPT_EDIT_TITLE);
		}
	}

	if (ImGui::CollapsingHeader("Counter"))
	{
		ImGui::Text("Enable");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(second_column_align);
		ImGui::Checkbox("##counter_enable", &burner->options.batch);

		ImGui::Text("Current");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(second_column_align);
		ImGui::InputInt("##counter_current", &burner->options.batch_current);

		ImGui::Text("Limit");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(second_column_align);
		ImGui::InputInt("##counter_limit", &burner->options.batch_limit);
	}

	ShowScriptEditor();

	ImGui::End();
}

static void ShowBinList(bool* p_opt_showbinwindow)
{
	ImGui::Begin("Select Bin", p_opt_showbinwindow);

	ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();

	const float cw = 5;
	const float il = cw + 15;
	const float tl = 200;
	const float ew = contentRegionAvail.x - 300;
	const float lw = contentRegionAvail.x + 300;
	const float lt = 1;
	const float lb = 2;
	const float mh = 20;
	
	ImGuiWindow* window = ImGui::GetCurrentWindow();

	for (uint32_t i = 0; i < BIN_BURN_PLAN_MAX_SIZE; ++i) {

		float item_top_y = window->DC.CursorPos.y;
		float item_top_x = window->DC.CursorPos.x;

		BinBurnPlan& plan = burner->burnPlans[i];

		ImGuiDCXAxisAlign(il);
		ImGui::Checkbox(IMIDTEXT("##Enable", i), &plan.enable);
		ImGui::SameLine();
		ImGui::Text(IMIDTEXT("Burn Bin #", i));

		if (!plan.enable) {
			ImGui::SameLine();
			ImGuiDCXAxisAlign(200);
			ImGui::Text(plan.fileName);
			ImGui::SameLine();
			ImGui::Text(plan.loadAddressText);
		} else {
			ImGuiDCXAxisAlign(il);
			ImGui::Text("FileName");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(tl);
			if (ImGui::InputTextEx(IMIDTEXT("##BurnFileName", i), "", plan.fileName, sizeof(plan.fileName), ImVec2(ew, 0.0f), ImGuiInputTextFlags_EnterReturnsTrue)) {
				std::string tempStr(plan.fileName);
				std::string tempStrGBK = utils::utf8_to_gbk(tempStr);

				if (std::filesystem::is_regular_file(tempStrGBK)) {
					plan.fileSize = static_cast<uint32_t>(std::filesystem::file_size(plan.fileName));
				} else {
					plan.fileSize = 0;
				}
			};
			ImGui::SameLine();
			if (ImGui::Button(IMIDTEXT("Open##Open", i)))
				ifd::FileDialog::Instance().Open(IMIDTEXT("BurnFile", i), "Choose firmware", ".*", false);

			ImGuiDCXAxisAlign(il);
			ImGui::Text("Load Address");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(tl);
			if (ImGui::InputTextEx(IMIDTEXT("##LoadAddressInput", i), "", plan.loadAddressText, sizeof(plan.loadAddressText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_EnterReturnsTrue)) {

			}
			ImGui::SameLine();
			if (plan.fileSize == 0) {
				ImGui::Text("Error: file not exists");
			} else {
				ImGui::Text((std::string("OK. file size = ") + std::to_string(plan.fileSize) + "B").c_str());
			}

			// file dialogs
			if (ifd::FileDialog::Instance().IsDone(IMIDTEXT("BurnFile", i))) {
				if (ifd::FileDialog::Instance().HasResult()) {
					const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
					if (res.size() > 0) {
						std::filesystem::path path = res[0];
						std::string pathStr = path.u8string();
						strcpy(plan.fileName, pathStr.c_str());
					}
				}
				ifd::FileDialog::Instance().Close();
			}

			// Bin file path
			auto path = get_file_path(plan.fileName);
			if (std::filesystem::is_regular_file(path))
				plan.fileSize = static_cast<uint32_t>(std::filesystem::file_size(path));
			else
				plan.fileSize = 0;

			// Load address
			if (plan.loadAddressText[0] == '\0') {
				plan.loadAddress = 0;
				strcpy(plan.loadAddressText, "0");
			} else {
				if (plan.loadAddressText[0] == '0' && plan.loadAddressText[1] == 'x') {
					sscanf_s(plan.loadAddressText, "%x", &plan.loadAddress);
				}
				else {
					plan.loadAddress = std::stoi(plan.loadAddressText);
				}
			}
		}

		float item_bottom_y = window->DC.CursorPos.y;

		window->DrawList->AddRectFilled(ImVec2(item_top_x, item_top_y), ImVec2(item_top_x + cw, item_bottom_y), plan.color);
		window->DrawList->AddRectFilled(ImVec2(item_top_x, item_bottom_y + lt), ImVec2(item_top_x + lw, item_bottom_y + lb), 0xFFB6B6B6);

		ImGuiDCYMargin(mh);
	}
	ImGui::End();
}

static void ShowRootWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

	if (opt_fullscreen) {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	}
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("-", p_open, window_flags);

	if (opt_fullscreen) {
		ImGui::PopStyleVar();	// WindowBorderSize
		ImGui::PopStyleVar();	// WindowRounding
	}
	ImGui::PopStyleVar();	// WindowPadding

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_AutoHideTabBar);

	ShowRootWindowMenu();

	ImGui::End();

	ShowBinList(&opt_showbinwindow);
	ShowPortConfig(&opt_showportwindow);
	ShowLog(&opt_showlogwindow);
	ShowConfigPanel(&opt_showconfigwindow);

	ShowBurnPanel();

	if (opt_showdemowindow) {
		ImGui::ShowDemoWindow(&opt_showdemowindow);
	}
}

static int main_gui()
{
	ShowRootWindow(&show_root_window);

	burner->SaveConfig();

    return 0;
}

#define APP_TITLE           "Multi Burner"

#define BACKEND_GL3

#ifdef BACKEND_GL3
#include "gui_glue_gl3.cpp"
#else
#include "gui_glue_dx12.cpp"
#endif


int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int nShowCmd)
{
    return main_(__argc, __argv);
}