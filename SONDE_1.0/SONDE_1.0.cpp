#include "stdafx.h"
#include <cstdio>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <string>
#include "bessel.h"
#include <iomanip>
#include <cmath>
#include <complex> 
#include "variable.h"
#include "function.h"
#include <vector>
#include <bitset>
#include "SONDE_1.0.h"

using namespace std;
using namespace System;
using namespace System::Windows::Forms;
using namespace System::Windows::Forms::DataVisualization::Charting;
using namespace System::Runtime::InteropServices;
double eps = 1;

HINSTANCE SONDE_3_C;// Объявляем идентификатор библиотеки
typedef int(*Create_inf_cyl_Pallete)(const char *, const char *, bool*, uint32_t* );
typedef int(*Create_vzz_2layer_Pallete)(void *, void *, bool *, uint32_t *);
typedef int(*Sonde_set)(const char *, const char *);
typedef int(*Borehole_offset)(float, int );
typedef int(*Get_express_data)(void *, PHASE *, Ro *rho, int );
typedef int(*Get_Phase)(void *, PHASE*, int);
typedef int(*Get_condition)(void *, uint32_t *, int);
typedef uint8_t(*Simmetry)(PHASE*, PHASE*, uint32_t);
typedef int(*Calculate_Rho_AF)(PHASE*, Ro *, float, int, int, int, SERVICE*);
typedef int(*Calculate_Rho_AS)(PHASE*, Ro *, int, int, SERVICE*);
typedef int(*Ph_shift_smt_ph)(PHASE *, Ro *, PHASE *);
typedef int(*Ph_shift_smt_ro)(Ro *, Ro *, PHASE *);
typedef int(*Ro_corr_ref_point)(const char *, Ro *, Ro *,Ro *, Ro *);
typedef int(*Ph_smt_ro)(Ro *, PHASE *);
typedef void(*Debug_mode)(bool);
typedef int(*Calculate_Rho_Doll_GR)(PHASE*, Ro *);

Create_inf_cyl_Pallete create_inf_cyl_Pallete;
Create_vzz_2layer_Pallete create_vzz_2layer_Pallete;
Sonde_set  sonde_set;  
Borehole_offset  borehole_offset;  
Get_express_data get_express_data;
Get_Phase get_Phase;
Get_condition get_condition;
Simmetry simmetry;
Calculate_Rho_AF  calculate_Rho_AF;
Calculate_Rho_AS  calculate_Rho_AS;
Ph_shift_smt_ph ph_shift_smt_ph;
Ph_shift_smt_ro ph_shift_smt_ro;
Ro_corr_ref_point ro_corr_ref_point;
Ph_smt_ro ph_smt_ro;
Debug_mode debug_mode;
Calculate_Rho_Doll_GR calculate_Rho_Doll_GR;

struct PHASE phase = { 0 }, phase_express = { 0 }, phase_smt = { 0 }, phase_pen, phase_smt_2043, phase_smt_2043_corr, phase_smt_1923, Phase_shift = { 0 };
struct Ro ro_express = { 0 }, ro_need, ro_2043, ro_AF = { 0 }, ro_required;
uint32_t condition = 0;
int shift = 0;
struct GP_DATA gp_data;
struct Ro Ro_3c, Ro_3c_AS, Ro_3c_AF;
SERVICE  service_AF;
int D_bhole_nom = 150;
float sigma_bhole = 0;
float ro_bh = 0;

const char * Metro_name = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\metro_autonm_5Tx.bin";
//const char * Metro_name =   "C:\\EXP\\NEW_DLL_TEST\\metro_107.bin";

//const char* Data_Name = "C:\\EXP\\NEW_DLL_TEST\\LWD_106_NEW.DEV";
//const char* Data_Name = "C:\\EXP\\NEW_DLL_TEST\\autonom_5Tx_KIS.DEV";
//const char* Data_Name = "D:\\InducRAM_107.DEV";
const char* Data_Name = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\autonom_5Tx.DEV";

const char *Pallete_dir = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\PALLETE\\";
const char * iсp_file_name = "D:\\PALLETE\\INF_CYL_PALLETE\\inf_syl_pallete_exp.icp";



INF_CYL_PALLETE_FILE_HEADER header = { 0, };
vector<INF_CYL_PALLETE_R > inf_cyl_pallete_r;
ifstream pallete_in;
ifstream fin;
ofstream fout;
GP_METROLOGY metro = { 0 };
string line;
float inf_cyl_ph[50][4], rzp[50], buff;
float exp_zp_ph[8][4];
string GetLastErrorText(DWORD errorCode) {
	LPSTR messageBuffer = NULL;
	DWORD size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&messageBuffer,
		0,
		NULL);

	string message = size && messageBuffer ? string(messageBuffer, size) : string("unknown error");
	if (messageBuffer) LocalFree(messageBuffer);
	return message;
}

bool TryLoadSondeLibrary(const string& dllPath) {
	SONDE_3_C = LoadLibraryA(dllPath.c_str());
	if (SONDE_3_C != NULL) {
		cout << "load the library '" << dllPath << "' ok " << endl;
		return true;
	}

	DWORD errorCode = GetLastError();
	cout << "Unable to load the library '" << dllPath << "'. Win32 error " << errorCode << ": " << GetLastErrorText(errorCode) << endl;
	return false;
}

bool LoadSondeLibrary() {
	const char* dllName = "SONDE_DLL_1.0.dll";
	char exePath[MAX_PATH] = { 0 };
	string exeDir;
	if (GetModuleFileNameA(NULL, exePath, MAX_PATH) > 0) {
		exeDir = exePath;
		size_t slash = exeDir.find_last_of("\\/");
		if (slash != string::npos) exeDir = exeDir.substr(0, slash);
	}

	vector<string> candidates;
	if (!exeDir.empty()) candidates.push_back(exeDir + "\\" + dllName);
	candidates.push_back(dllName);
	candidates.push_back("C:\\WORK\\repos\\DLL\\SONDE_DLL\\SONDE_DLL_NEW\\Release\\SONDE_DLL_1.0.dll");
	if (!exeDir.empty()) candidates.push_back(exeDir + "\\..\\Debug\\" + dllName);

	for (size_t i = 0; i < candidates.size(); i++) {
		if (TryLoadSondeLibrary(candidates[i])) return true;
	}

	return false;
}

bool RequiredDllFunctionsLoaded() {
	bool ok = true;
	if (!sonde_set) ok = false;
	if (!get_express_data) ok = false;
	if (!get_Phase) ok = false;
	if (!get_condition) ok = false;
	if (!simmetry) ok = false;
	if (!calculate_Rho_AF) ok = false;
	if (!ph_shift_smt_ph) ok = false;
	if (!ph_shift_smt_ro) ok = false;
	if (!ro_corr_ref_point) ok = false;
	if (!ph_smt_ro) ok = false;
	if (!debug_mode) ok = false;
	if (!calculate_Rho_Doll_GR) ok = false;
	return ok;
}
[STAThread]
int main()
{
	cout << " header_size " << sizeof(header) << endl;
	//create_bessel_pallete();

#pragma region инициализация формы для отображения графиков

	Application::EnableVisualStyles();
	int width = 1200;

	Form^ form = gcnew Form;
	form->Text = " 'RAMON FILM' production";
	form->ClientSize = System::Drawing::Size(1200, 820);
	form->AutoScroll = true;

	Chart^  chart1 = (gcnew Chart());
	form->Controls->Add(chart1);
	chart1->Size = System::Drawing::Size(width, 500);
	chart1->Location = System::Drawing::Point(5, 0);
	chart1->ChartAreas->Add("ChartArea1");
	chart1->Legends->Add("Legend1");

	chart1->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart1->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart1->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart1->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart1->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;
	chart1->ChartAreas["ChartArea1"]->CursorY->Interval = 0.01;


	for (int i = 0; i < 10; i++) {
		chart1->Series->Add("series" + i);
		//chart1->Series[i]->LegendText = L" " + i;
		chart1->Series[i]->ChartType = SeriesChartType::Line;
		
		chart1->Series[i]->BorderWidth = 2;
	}
	//chart1->Series[0]->LegendText = L"T1 " ;
	//chart1->Series[1]->LegendText = L"T2 ";
	//chart1->Series[2]->LegendText = L"T3 ";
	//chart1->Series[3]->LegendText = L"T4 ";
	//chart1->Series[4]->LegendText = L"Ro_pz ";
	//chart1->Series[5]->LegendText = L"Ro_st ";

	//chart1->Series[0]->Color = System::Drawing::Color::Green;
	//chart1->Series[1]->Color = System::Drawing::Color::Yellow;
	//chart1->Series[2]->Color = System::Drawing::Color::Red;
	//chart1->Series[3]->Color = System::Drawing::Color::Blue;
	//chart1->Series[4]->Color = System::Drawing::Color::DarkGray;
	//chart1->Series[5]->Color = System::Drawing::Color::Black;

	Chart^  chart2 = (gcnew Chart());
	form->Controls->Add(chart2);
	chart2->Size = System::Drawing::Size(width, 500);
	chart2->Location = System::Drawing::Point(5, 500);
	chart2->ChartAreas->Add("ChartArea1");
	chart2->Legends->Add("Legend1");
	chart2->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart2->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart2->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart2->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart2->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;



	for (int i = 0; i < 10; i++) {

		chart2->Series->Add("series" + i);
		chart2->Series[i]->LegendText = L" " + i;
		chart2->Series[i]->ChartType = SeriesChartType::Line;
		chart2->Series[i]->BorderWidth = 2;

	}

	Chart^  chart3 = (gcnew Chart());
	form->Controls->Add(chart3);
	chart3->Size = System::Drawing::Size(width, 500);
	chart3->Location = System::Drawing::Point(5, 1000);
	chart3->ChartAreas->Add("ChartArea1");
	chart3->Legends->Add("Legend1");
	chart3->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart3->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart3->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart3->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart3->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 10; i++) {
		chart3->Series->Add("series" + i);
		chart3->Series[i]->LegendText = L" " + i;
		chart3->Series[i]->ChartType = SeriesChartType::Line;
		chart3->Series[i]->BorderWidth = 2;
	}

	Chart^  chart4 = (gcnew Chart());
	form->Controls->Add(chart4);
	chart4->Size = System::Drawing::Size(width, 500);
	chart4->Location = System::Drawing::Point(5, 1500);
	chart4->ChartAreas->Add("ChartArea1");
	chart4->Legends->Add("Legend1");
	chart4->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart4->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart4->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart4->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart4->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 10; i++) {

		chart4->Series->Add("series" + i);
		chart4->Series[i]->LegendText = L" " + i;
		chart4->Series[i]->ChartType = SeriesChartType::Line;
		chart4->Series[i]->BorderWidth = 2;
	}

	Chart^  chart5 = (gcnew Chart());
	form->Controls->Add(chart5);
	chart5->Size = System::Drawing::Size(width, 500);
	chart5->Location = System::Drawing::Point(5, 2000);
	chart5->ChartAreas->Add("ChartArea1");
	chart5->Legends->Add("Legend1");
	chart5->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart5->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart5->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart5->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart5->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 8; i++) {
		chart5->Series->Add("series" + i);
		chart5->Series[i]->LegendText = L" " + i;
		chart5->Series[i]->ChartType = SeriesChartType::Line;
		chart5->Series[i]->BorderWidth = 2;
	}

	Chart^  chart6 = (gcnew Chart());
	form->Controls->Add(chart6);
	chart6->Size = System::Drawing::Size(width, 500);
	chart6->Location = System::Drawing::Point(5, 2500);
	chart6->ChartAreas->Add("ChartArea1");
	chart6->Legends->Add("Legend1");
	chart6->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart6->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart6->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart6->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart6->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 8; i++) {

		chart6->Series->Add("series" + i);
		chart6->Series[i]->LegendText = L" " + i;
		chart6->Series[i]->ChartType = SeriesChartType::Line;
		chart6->Series[i]->BorderWidth = 2;
	}

	Chart^  chart7 = (gcnew Chart());
	form->Controls->Add(chart7);
	chart7->Size = System::Drawing::Size(width, 500);
	chart7->Location = System::Drawing::Point(5, 3000);
	chart7->ChartAreas->Add("ChartArea1");
	chart7->Legends->Add("Legend1");
	chart7->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart7->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart7->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart7->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart7->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 8; i++) {
		chart7->Series->Add("series" + i);
		chart7->Series[i]->LegendText = L" " + i;
		chart7->Series[i]->ChartType = SeriesChartType::Line;
		chart7->Series[i]->BorderWidth = 2;
	}

	Chart^  chart8 = (gcnew Chart());
	form->Controls->Add(chart8);
	chart8->Size = System::Drawing::Size(width, 500);
	chart8->Location = System::Drawing::Point(5, 3500);
	chart8->ChartAreas->Add("ChartArea1");
	chart8->Legends->Add("Legend1");
	chart8->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart8->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart8->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart8->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart8->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 8; i++) {

		chart8->Series->Add("series" + i);
		chart8->Series[i]->LegendText = L" " + i;
		chart8->Series[i]->ChartType = SeriesChartType::Line;
		chart8->Series[i]->BorderWidth = 2;
	}

	Chart^  chart9 = (gcnew Chart());
	form->Controls->Add(chart9);
	chart9->Size = System::Drawing::Size(width, 500);
	chart9->Location = System::Drawing::Point(5, 4000);
	chart9->ChartAreas->Add("ChartArea1");
	chart9->Legends->Add("Legend1");
	chart9->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart9->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart9->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart9->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart9->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 10; i++) {

		chart9->Series->Add("series" + i);
		chart9->Series[i]->LegendText = L" " + i;
		chart9->Series[i]->ChartType = SeriesChartType::Line;
		chart9->Series[i]->BorderWidth = 2;
	}

	Chart^  chart10 = (gcnew Chart());
	form->Controls->Add(chart10);
	chart10->Size = System::Drawing::Size(width, 500);
	chart10->Location = System::Drawing::Point(5, 4500);
	chart10->ChartAreas->Add("ChartArea1");
	chart10->Legends->Add("Legend1");
	chart10->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart10->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart10->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart10->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart10->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 10; i++) {

		chart10->Series->Add("series" + i);
		chart10->Series[i]->LegendText = L" " + i;
		chart10->Series[i]->ChartType = SeriesChartType::Line;
		chart10->Series[i]->BorderWidth = 2;
	}

#pragma endregion 

#pragma region library DLL
	                             
	if (!LoadSondeLibrary()) {
		cout << "SONDE DLL was not loaded. Put a compatible SONDE_DLL_1.0.dll next to SONDE_1.0.exe or update the DLL path." << endl;
		getchar();
		return 1;
	}
	create_inf_cyl_Pallete = (Create_inf_cyl_Pallete)GetProcAddress(SONDE_3_C, "create_inf_cyl_Pallete");
	if (!create_inf_cyl_Pallete)	cout << "Unable to find the function 'create_inf_cyl_Pallete' " << endl;
	else cout << "create_inf_cyl_Pallete ok " << endl;

	create_vzz_2layer_Pallete = (Create_vzz_2layer_Pallete)GetProcAddress(SONDE_3_C, "create_vzz_2layer_Pallete");
	if (!create_vzz_2layer_Pallete)	cout << "Unable to find the function 'create_vzz_2layer_Pallete' " << endl;
	else cout << "create_vzz_2layer_Pallete ok " << endl;
	
	sonde_set = (Sonde_set)GetProcAddress(SONDE_3_C, "sonde_set");
	if (!sonde_set)	cout << "Unable to find the function 'sonde_set' " << endl;
	else cout << "sonde_set is  ok" << endl;

	borehole_offset = (Borehole_offset)GetProcAddress(SONDE_3_C, "borehole_offset");
	if (!borehole_offset)	cout << "Unable to find the function 'borehole_offset' " << endl;
	else cout << "borehole_offset is  ok" << endl;

	get_express_data = (Get_express_data)GetProcAddress(SONDE_3_C, "get_express_data");
	if (!get_express_data)	cout << "Unable to find the function 'get_express_data' " << endl;
	else cout << "get_express_data is  ok" << endl;
	
	get_Phase = (Get_Phase)GetProcAddress(SONDE_3_C, "get_Phase");
	if (!get_Phase)	// Проверяем полученный указатель
		cout << "Unable to find the function 'get_Phase' " << endl;
	else cout << "get_Phase is  ok" << endl;

	get_condition = (Get_condition)GetProcAddress(SONDE_3_C, "get_condition");
	if (!get_condition)	// Проверяем полученный указатель
		cout << "Unable to find the function 'get_condition' " << endl;
	else cout << "get_condition is  ok" << endl;

	simmetry = (Simmetry)GetProcAddress(SONDE_3_C, "simmetry");
	if (!simmetry)	// Проверяем полученный указатель
		cout << "Unable to find the function 'simmetry' " << endl;
	else cout << "simmetry is  ok" << endl;

	calculate_Rho_AF = (Calculate_Rho_AF)GetProcAddress(SONDE_3_C, "calculate_Rho_AF");
	if (!calculate_Rho_AF)	// Проверяем полученный указатель
		cout << "Unable to find the function 'calculate_Rho_AF' " << endl;
	else cout << "calculate_Rho_AF is  ok" << endl;

	calculate_Rho_AS = (Calculate_Rho_AS)GetProcAddress(SONDE_3_C, "calculate_Rho_AS");
	if (!calculate_Rho_AS)	// Проверяем полученный указатель
		cout << "Unable to find the function 'calculate_Rho_AS' " << endl;
	else cout << "calculate_Rho_AS is  ok" << endl;

	ph_shift_smt_ph = (Ph_shift_smt_ph)GetProcAddress(SONDE_3_C, "ph_shift_smt_ph");
	if (!ph_shift_smt_ph)	// Проверяем полученный указатель
		cout << "Unable to find the function 'ph_shift_smt_ph' " << endl;
	else cout << "ph_shift_smt_ph is  ok" << endl;

	ph_shift_smt_ro = (Ph_shift_smt_ro)GetProcAddress(SONDE_3_C, "ph_shift_smt_ro");
	if (!ph_shift_smt_ro)	// Проверяем полученный указатель
		cout << "Unable to find the function 'ph_shift_smt_ro' " << endl;
	else cout << "ph_shift_smt_ro is  ok" << endl;

	ro_corr_ref_point = (Ro_corr_ref_point)GetProcAddress(SONDE_3_C, "ro_corr_ref_point");
	if (!ro_corr_ref_point)	// Проверяем полученный указатель
		cout << "Unable to find the function 'ro_corr_ref_point' " << endl;
	else cout << "ro_corr_ref_point is  ok" << endl;

	ph_smt_ro = (Ph_smt_ro)GetProcAddress(SONDE_3_C, "ph_smt_ro");
	if (!ph_smt_ro)	// Проверяем полученный указатель
		cout << "Unable to find the function 'ph_smt_ph' " << endl;
	else cout << "ph_smt_ro is  ok" << endl;

	debug_mode = (Debug_mode)GetProcAddress(SONDE_3_C, "debug_mode");
	if (!debug_mode)	// Проверяем полученный указатель
		cout << "Unable to find the function 'debug_mode' " << endl;
	else cout << "debug_mode is  ok" << endl;

	calculate_Rho_Doll_GR = (Calculate_Rho_Doll_GR)GetProcAddress(SONDE_3_C, "calculate_Rho_Doll_GR");
	if (!calculate_Rho_Doll_GR)	// Проверяем полученный указатель
		cout << "Unable to find the function 'calculate_Rho_Doll_GR' " << endl;
	else cout << "calculate_Rho_Doll_GR is  ok" << endl;

#pragma endregion 

	if (!RequiredDllFunctionsLoaded()) {
		cout << "SONDE DLL is loaded, but it does not contain all functions required by this test project." << endl;
		cout << "Use the updated SONDE_DLL_1.0.dll from the DLL project with the same platform as this executable." << endl;
		FreeLibrary(SONDE_3_C);
		getchar();
		return 1;
	}

	debug_mode(1);
	/*
	pallete_in.open(iсp_file_name, ios::binary);//читаем header
	if (!pallete_in.is_open()) {
		cout << "Vzz_inf_cyl_pallete file not open " << endl;
	}
	pallete_in.read((char*)&header, sizeof(header));
	while (!pallete_in.eof()) {
		INF_CYL_PALLETE_R buff;
		pallete_in.read((char*)&buff, sizeof(INF_CYL_PALLETE_R));
		inf_cyl_pallete_r.push_back(buff);
		//cout << buff.N << endl;
	}
	pallete_in.close();

	int N = 1750;
	cout << " N " << inf_cyl_pallete_r[N].N << " Ro_p " << inf_cyl_pallete_r[N].Ro_p << " Ro_zp " << inf_cyl_pallete_r[N].Ro_zp << endl;

	for (int n_r_zp = 0; n_r_zp < 100; n_r_zp++) {
		float PH[2][5] = { 0.0, };
		for (int freq = 0; freq < 2; freq++) {
			halfToFloat(inf_cyl_pallete_r[N].PH[n_r_zp][freq], (uint32_t *)PH[freq], 5);
		}
		for (int Tx = 0; Tx < 5; Tx++) {
			chart1->Series[Tx]->Points->AddXY(n_r_zp, PH[_400_kGz][Tx] * mG);
		}
	}
	*/
	
	
	

	bool *start_stop = new bool;
	*start_stop = true;
	uint32_t *persent = new uint32_t;
	 //create_inf_cyl_Pallete(Metro_name, iсp_file_name, start_stop, persent) ;

	int file_frames = 0;
	int struct_size = 11 + sizeof(GP_DATA);
	int start_frame = 0;
	int end_frame = 0;
	float Ro_bh = 0.0f;
	int D_bh_mm = 0;
	int N_Tx = 5;
	//требуемое УЭС
	for (int freq = 0; freq < 2; freq++) {
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			ro_need.Ro[freq][Tx] = 40.0f;
		}
	}

	fin.open(Metro_name, ios::binary);//открываем  файл данных
	if (fin.is_open()) {
		fin.read((char*)&metro, sizeof(GP_DATA));
		cout << "metrofile is open " << file_frames << endl;
	}
	fin.close();

	cout << "sonde set " << sonde_set(Metro_name, Pallete_dir) << endl;

	fin.open(Data_Name, ios::binary);//открываем  файл данных
	if (fin.is_open()) {
		fin.seekg(0, ios::end);
		file_frames = int(fin.tellg()) / struct_size;
		cout << "file_frames " << file_frames << endl;
	}
	fin.close();

	fin.open(Data_Name, ios::binary);//открываем  файл данных
	for (int n = 0; n < file_frames; n++) {
		gp_data = READ_BLOK_GP_DATA_DEV(fin, n);

		int get_ph_result = get_Phase(&gp_data, &phase, shift);
		//cout << "get_ph_result "<< get_ph_result <<endl;
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			//chart1->Series[Tx]->Points->AddXY(n, gp_data.DELTA_PH[_400_kGz][Tx] * mG);
			//chart2->Series[Tx]->Points->AddXY(n, phase.Phase[_400_kGz][Tx] * mG);
			//chart3->Series[Tx]->Points->AddXY(n, gp_data.DELTA_PH[_2000_kGz][Tx] * mG);
			//chart4->Series[Tx]->Points->AddXY(n, phase.Phase[_2000_kGz][Tx] * mG);
		}
		get_express_data(&gp_data, &phase_express, &ro_express, shift);
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			chart1->Series[Tx]->Points->AddXY(n, phase_express.Phase[_400_kGz][Tx] * mG);
			chart2->Series[Tx]->Points->AddXY(n, phase_express.Phase[_2000_kGz][Tx] * mG);
			chart3->Series[Tx]->Points->AddXY(n, ro_express.Ro[_400_kGz][Tx]);
			chart4->Series[Tx]->Points->AddXY(n, ro_express.Ro[_2000_kGz][Tx]) ;
		}
		get_condition(&gp_data, &condition, shift);
		//cout << gp_data.condition << " "  << condition << endl;
		//cout << std::bitset<32>(gp_data.condition) << endl;
		
		simmetry(&phase, &phase_smt, 0xffffffff);
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			//chart1->Series[Tx]->Points->AddXY(n, phase_smt.Phase[_400_kGz][Tx] * mG);
			//chart2->Series[Tx]->Points->AddXY(n, phase_smt.Phase[_2000_kGz][Tx] * mG);
		}

		
		int pz_400 = 0; 
		int pz_2000 = 0;

		calculate_Rho_AF(&phase_smt, &ro_AF, ro_bh, D_bhole_nom, pz_400, pz_2000, &service_AF);
		calculate_Rho_Doll_GR(&phase_smt, &Ro_3c);
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			//chart5->Series[Tx]->Points->AddXY(n, ro_AF.Ro[_400_kGz][Tx]);
			//chart6->Series[Tx]->Points->AddXY(n, ro_AF.Ro[_2000_kGz][Tx]);
			//chart5->Series[Tx]->Points->AddXY(n, Ro_3c.Ro[_400_kGz][Tx]);
			//chart6->Series[Tx]->Points->AddXY(n, Ro_3c.Ro[_2000_kGz][Tx]);
		}

		


		ph_smt_ro(&ro_AF, &phase_pen);
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			//chart1->Series[Tx]->Points->AddXY(n, phase_smt.Phase[_400_kGz][Tx] * mG);
			//chart1->Series[Tx + 4]->Points->AddXY(n, 10 + phase_pen.Phase[_400_kGz][Tx] * mG);
			//chart2->Series[Tx]->Points->AddXY(n, phase_smt.Phase[_2000_kGz][Tx] * mG);
			//chart2->Series[Tx+4]->Points->AddXY(n, 10 + phase_pen.Phase[_2000_kGz][Tx] * mG);
		}


		if (n == 2000) {
			phase_smt_1923 = phase_smt;
			
		}
		if (n == 2000) {
			phase_smt_2043 = phase_smt;
			ro_2043 = ro_AF;
		}

		ro_corr_ref_point(Metro_name, &ro_2043, &ro_need, &ro_AF, &ro_required);
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			//chart1->Series[Tx]->Points->AddXY(n, ro_AF.Ro[_400_kGz][Tx]);
			//chart1->Series[Tx+4]->Points->AddXY(n, ro_required.Ro[_400_kGz][Tx]);
			//chart2->Series[Tx]->Points->AddXY(n, ro_AF.Ro[_2000_kGz][Tx]);
			//chart2->Series[Tx+4]->Points->AddXY(n, ro_required.Ro[_2000_kGz][Tx]);
		}

	}
	fin.close();


	
	//вводим фазу и требуемое УЭС для данной точки и получаем необходимую фазовую поправку
	cout << "ph_smt_806 ";
	for (int freq = 0; freq < 2; freq++) {
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			cout << phase_smt_1923.Phase[freq][Tx] * mG << " ";
		}
	}
	cout << endl;
	
	ph_shift_smt_ph(&phase_smt_1923, &ro_need, &Phase_shift);
	cout << "ph_shift_smt_ph ";
	for (int freq = 0; freq < 2; freq++) {
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			cout << Phase_shift.Phase[freq][Tx] * mG << " ";
		}
	}
	cout << endl;
	//отнимаем от фазы поправку
	for (int freq = 0; freq < 2; freq++) {
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			phase_smt_1923.Phase[freq][Tx] -= Phase_shift.Phase[freq][Tx] ;
		}
	}
	//считаем УЭС от фазы с поправкой 
	calculate_Rho_AF(&phase_smt_1923, &ro_AF, ro_bh, D_bhole_nom, 0, 0, &service_AF);
	for (int freq = 0; freq < 2; freq++) {
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			cout << ro_AF.Ro[freq][Tx] << " ";
		}
	}
	cout << endl;
	//вводим УЭС и  требуемое УЭС для данной точки и получаем необходимую фазовую поправку///////////////////////////////////////////////////////////////////////////////////////
	ph_shift_smt_ro(&ro_2043, &ro_need, &Phase_shift);
	cout << "ph_shift_smt_ro ";
	for (int freq = 0; freq < 2; freq++) {
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			cout << Phase_shift.Phase[freq][Tx] * mG << " ";
		}
	}
	cout << endl;
	//отнимаем от фазы поправку
	for (int freq = 0; freq < 2; freq++) {
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			phase_smt_2043_corr.Phase[freq][Tx] = phase_smt_2043.Phase[freq][Tx] - Phase_shift.Phase[freq][Tx];
		}
	}
	//считаем УЭС от фазы с поправкой 
	calculate_Rho_AF(&phase_smt_2043_corr, &ro_AF, ro_bh, D_bhole_nom, 0, 0, &service_AF);
	for (int freq = 0; freq < 2; freq++) {
		for (int Tx = 0; Tx < N_Tx; Tx++) {
			cout << ro_AF.Ro[freq][Tx] << " ";
		}
	}
	cout << endl;

	
	///////////////////////////////////////////////////////////////////////////
	
	FreeLibrary(SONDE_3_C);	// Освобождаем библиотеку
	Application::Run(form);
	getchar(); getchar();
	return 0;

}

