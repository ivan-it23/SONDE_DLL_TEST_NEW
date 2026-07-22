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
#include <cstring>
#include <msclr/marshal_cppstd.h>
#include "SONDE_1.0.h"

using namespace std;
using namespace System;
using namespace System::Windows::Forms;
using namespace System::Windows::Forms::DataVisualization::Charting;
using namespace System::Runtime::InteropServices;
double eps = 1;

HINSTANCE SONDE_3_C;// Объявляем идентификатор библиотеки
typedef int(*Sonde_set)(const char *, const char *);
typedef int(*Borehole_offset)(float, int );
typedef int(*Get_express_data)(void *, PHASE *, Ro *rho, int );
typedef int(*Get_Phase)(void *, PHASE*, int);
typedef int(*Get_condition)(void *, uint32_t *, int);
typedef int(*Simmetry)(PHASE*, PHASE*, uint32_t);
typedef int(*Calculate_Rho_AF)(PHASE*, Ro *, float, int, int, int, SERVICE*);
typedef int(*Ph_shift_smt_ph)(PHASE *, Ro *, PHASE *);
typedef int(*Ph_shift_smt_ro)(Ro *, Ro *, PHASE *);
typedef int(*Ro_corr_ref_point)(const char *, Ro *, Ro *,Ro *, Ro *);
typedef int(*Ph_smt_ro)(Ro *, PHASE *);
typedef void(*Debug_mode)(bool);
typedef int(*Calculate_Rho_Doll_GR)(PHASE*, Ro *);
typedef int(*Ph_smt_zp)(Ro *, PHASE *);
typedef int(*Get_data_file_info)(const char *, uint32_t *, int *, uint32_t *);
typedef const char*(*Get_sonde_last_error)();

Sonde_set  sonde_set;  
Borehole_offset  borehole_offset;  
Get_express_data get_express_data;
Get_Phase get_Phase;
Get_condition get_condition;
Simmetry simmetry;
Calculate_Rho_AF  calculate_Rho_AF;
Ph_shift_smt_ph ph_shift_smt_ph;
Ph_shift_smt_ro ph_shift_smt_ro;
Ro_corr_ref_point ro_corr_ref_point;
Ph_smt_ro ph_smt_ro;
Debug_mode debug_mode;
Calculate_Rho_Doll_GR calculate_Rho_Doll_GR;
Ph_smt_zp ph_smt_zp;
Get_data_file_info get_data_file_info;
Get_sonde_last_error get_sonde_last_error;

struct PHASE phase = { 0 }, phase_express = { 0 }, phase_smt = { 0 }, phase_pen, phase_smt_2043, phase_smt_2043_corr, phase_smt_1923, Phase_shift = { 0 }, phase_zp = { 0 };
struct Ro ro_express = { 0 }, ro_need, ro_2043, ro_AF = { 0 }, ro_required;
uint32_t condition = 0;
int shift = 0;
struct GP_DATA gp_data;
struct Ro Ro_3c, Ro_3c_AS, Ro_3c_AF;
SERVICE  service_AF;
int D_bhole_nom = 150;
float sigma_bhole = 0;
float ro_bh = 0;
const int kNeuroWeightsNotFound = 203;
const int kFrameSignatureMismatch = 2;
const int kDataFileOpenError = 3;
const int kDataFileExtensionError = 4;
const int kDataFileLayoutError = 5;
const int kMetrologySizeError = 8;

// Кэш результатов вычисления фаз зоны проникновения для исключения повторного численного интегрирования.
struct CacheKey {
	int Ro_p_q;
	int Ro_zp_q;
	int R_zp_q;
	bool operator==(const CacheKey& other) const {
		return Ro_p_q == other.Ro_p_q && Ro_zp_q == other.Ro_zp_q && R_zp_q == other.R_zp_q;
	}
};

struct CacheEntry {
	CacheKey key;
	PHASE phase;
};

std::vector<CacheEntry> phase_zp_cache;
const float kQuantStepRo = 0.1f;
const float kQuantStepR = 1.0f;

// Вычисляет симметризованные фазы зоны проникновения с кэшированием по квантованным параметрам.
void ph_smt_zp_cached(Ro* ro_AF, PHASE* phase_zp) {
	CacheKey key;
	key.Ro_p_q = static_cast<int>(ro_AF->Ro_p[0] / kQuantStepRo);
	key.Ro_zp_q = static_cast<int>(ro_AF->Ro_zp[0] / kQuantStepRo);
	key.R_zp_q = static_cast<int>(ro_AF->R_zp[0] / kQuantStepR);

	for (const auto& entry : phase_zp_cache) {
		if (entry.key == key) {
			*phase_zp = entry.phase;
			return;
		}
	}

	ph_smt_zp(ro_AF, phase_zp);
	CacheEntry new_entry;
	new_entry.key = key;
	new_entry.phase = *phase_zp;
	phase_zp_cache.push_back(new_entry);
}

//const char * Metro_name = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\metro_LWD_109_008_2024_.bin";
//const char * Metro_name = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\metro_autonm_5Tx.bin";
const char* kDefaultMetroName = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\Metrology.bin";


//const char * Metro_name =   "C:\\EXP\\NEW_DLL_TEST\\metro_107.bin";


//const char* Data_Name = "C:\\EXP\\NEW_DLL_TEST\\LWD_106_NEW.DEV";
//const char* Data_Name = "C:\\EXP\\NEW_DLL_TEST\\autonom_5Tx_KIS.DEV";
//const char* Data_Name = "D:\\InducRAM_107.DEV";

//const char* Data_Name = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\IndRAM.DEV";
//const char* Data_Name = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\IndRAM_cut_0_1300_02_06_2026.DEV";
//const char* Data_Name = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\InducRAM_cut_0_1400_09_06_2026_cut_1200_2367_09_06_2026.DEV";
const char* kDefaultDataName = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\Copy(3).DEV";
//const char* Data_Name = "C:\\Users\\Admin\\Desktop\\SONDE_DLL_TEST_NEW\\autonom_5Tx.DEV";

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
	if (!ph_smt_zp) ok = false;
	if (!get_data_file_info) ok = false;
	if (!get_sonde_last_error) ok = false;
	return ok;
}

static String^ DllErrorMessage(int code, String^ context) {
	String^ message = context + L" Код ошибки: " + Convert::ToString(code) + L".";
	if (get_sonde_last_error != nullptr) {
		const char* detail = get_sonde_last_error();
		if (detail != nullptr && detail[0] != '\0')
			message += L"\n\nПодробности DLL: " + gcnew String(detail);
	}
	return message;
}
// Нормализация фазы (рад) в диапазон (-pi, pi]. Повторяет NormalizePhase
// эталонного проекта Amk-Horizon-New-Chart для режима вычисления на компьютере.
static float NormalizePhaseRad(float ph) {
	const float pi = 3.14159265358979323846f;
	while (ph > pi) ph -= 2.0f * pi;
	while (ph <= -pi) ph += 2.0f * pi;
	return ph;
}

// Декодирование сигнатуры зонда. Повторяет логику get_sonde_id из SONDE_DLL_NEW
// (SondeCore.cpp): младшие 20 бит сигнатуры кодируют тип, число передатчиков,
// модификацию и номер прибора. Используется только для диагностики раскладки.
struct SONDE_ID_DBG { uint32_t type_, N_Tx, mod, number, type; };
static SONDE_ID_DBG DecodeSondeId(uint32_t signature) {
	SONDE_ID_DBG t;
	uint32_t buff = (signature << 12) >> 12;
	t.type_ = buff / 100000;
	t.N_Tx = (buff % 100000) / 10000;
	t.mod = (buff % 10000) / 1000;
	t.number = (buff % 1000);
	t.type = buff / 1000;
	return t;
}

// Печать массива [2][5] построчно по частотам для удобства визуальной сверки.
static void DumpFreqArray(std::ofstream &os, const char *name, const float arr[2][5]) {
	os << "  " << name << ":\n";
	os << "    [400 ]: ";
	for (int t = 0; t < 5; t++) os << std::setw(16) << arr[0][t] << " ";
	os << "\n    [2000]: ";
	for (int t = 0; t < 5; t++) os << std::setw(16) << arr[1][t] << " ";
	os << "\n";
}

// Диагностический дамп одной записи GP_DATA: декодированная сигнатура, скаляры,
// все массивы в трактовке [2][5], а также сырое представление 240 байт как
// последовательности float со смещениями. Позволяет увидеть рассинхрон
// раскладки 4-передатчикового прибора при чтении как [2][5].
static void DumpGpDataFrame(std::ofstream &os, int frameIndex, const GP_DATA &d) {
	SONDE_ID_DBG id = DecodeSondeId(d.signature);
	os << "================ FRAME " << frameIndex << " ================\n";
	os << "signature   = " << d.signature << " (0x" << std::hex << d.signature << std::dec << ")\n";
	os << "  decoded -> type_=" << id.type_ << " N_Tx=" << id.N_Tx
	   << " mod=" << id.mod << " number=" << id.number << " type=" << id.type << "\n";
	os << "condition   = " << d.condition << " (0x" << std::hex << d.condition << std::dec << ")\n";
	os << "frame       = " << d.frame << "\n";
	os << "temperature = " << d.temperature << "\n";
	DumpFreqArray(os, "rho_smt  ", d.rho_smt);
	DumpFreqArray(os, "phase_smt", d.phase_smt);
	DumpFreqArray(os, "AM_RX_1  ", d.AM_RX_1);
	DumpFreqArray(os, "AM_RX_2  ", d.AM_RX_2);
	DumpFreqArray(os, "DELTA_PH ", d.DELTA_PH);
	os << "  ZERO_AM_RX_1: " << d.ZERO_AM_RX_1[0] << " " << d.ZERO_AM_RX_1[1] << "\n";
	os << "  ZERO_AM_RX_2: " << d.ZERO_AM_RX_2[0] << " " << d.ZERO_AM_RX_2[1] << "\n";
	os << "  ZERO_dPH    : " << d.ZERO_dPH[0] << " " << d.ZERO_dPH[1] << "\n";

	// Сырое представление структуры как float с байтовыми смещениями: помогает
	// определить фактические границы массивов на диске независимо от трактовки [2][5].
	os << "  RAW floats (offset:value), 60 шт.:\n";
	const float *raw = reinterpret_cast<const float*>(&d);
	for (int i = 0; i < (int)(sizeof(GP_DATA) / sizeof(float)); i++) {
		os << "    off " << std::setw(3) << (i * 4) << " : " << std::setw(16) << raw[i];
		if ((i % 4) == 3) os << "\n";
	}
	os << "\n";
}

static void ClearCharts(cli::array<Chart^>^ charts) {
	for each (Chart^ chart in charts) {
		for each (Series^ series in chart->Series)
			series->Points->Clear();
	}
}

static int ProcessDataFiles(
	String^ metrologyPath,
	String^ dataPath,
	cli::array<Chart^>^ charts,
	ToolStripStatusLabel^ statusLabel,
	String^% errorMessage) {
	using msclr::interop::marshal_as;

	errorMessage = nullptr;
	ClearCharts(charts);
	phase_zp_cache.clear();
	memset(&phase, 0, sizeof(phase));
	memset(&phase_express, 0, sizeof(phase_express));
	memset(&phase_smt, 0, sizeof(phase_smt));
	memset(&phase_pen, 0, sizeof(phase_pen));
	memset(&ro_express, 0, sizeof(ro_express));
	memset(&ro_AF, 0, sizeof(ro_AF));
	memset(&Ro_3c, 0, sizeof(Ro_3c));
	memset(&service_AF, 0, sizeof(service_AF));
	condition = 0;

	const string metroName = marshal_as<string>(metrologyPath);
	const string dataName = marshal_as<string>(dataPath);

	GP_METROLOGY selectedMetro = {};
	ifstream metroInput(metroName.c_str(), ios::binary);
	if (!metroInput.is_open()) {
		errorMessage = L"Не удалось открыть файл метрологии:\n" + metrologyPath;
		return 1;
	}
	metroInput.seekg(0, ios::end);
	const streamoff metrologySize = metroInput.tellg();
	if (metrologySize != static_cast<streamoff>(sizeof(GP_METROLOGY))) {
		errorMessage = L"Файл метрологии должен содержать ровно 240 байт актуальной структуры. "
			L"Фактический размер: " + Convert::ToString(static_cast<long long>(metrologySize)) +
			L" байт.\n" + metrologyPath;
		return kMetrologySizeError;
	}
	metroInput.seekg(0, ios::beg);
	metroInput.read(reinterpret_cast<char*>(&selectedMetro), sizeof(GP_METROLOGY));
	if (!metroInput) {
		errorMessage = L"Файл метрологии повреждён или имеет неверный размер:\n" + metrologyPath;
		return 1;
	}
	metroInput.close();

	int sondeSetResult = sonde_set(metroName.c_str(), nullptr);
	if (sondeSetResult != 0) {
		errorMessage = DllErrorMessage(sondeSetResult,
			sondeSetResult == kNeuroWeightsNotFound
			? L"Для сигнатуры выбранного прибора отсутствует полный комплект нейросетевых весов."
			: L"Не удалось инициализировать DLL по выбранной метрологии.");
		return sondeSetResult;
	}

	uint32_t fileFrames = 0;
	uint32_t dataSignature = 0;
	int frameHeaderSize = 0;
	int dataInfoResult = get_data_file_info(
		dataName.c_str(), &fileFrames, &frameHeaderSize, &dataSignature);
	if (dataInfoResult != 0) {
		switch (dataInfoResult) {
		case kFrameSignatureMismatch:
			errorMessage = L"Файл метрологии и файл данных относятся к разным приборам. "
				L"Выберите файлы одного и того же конкретного прибора.";
			break;
		case kDataFileOpenError:
			errorMessage = L"Не удалось открыть файл данных:\n" + dataPath;
			break;
		case kDataFileExtensionError:
			errorMessage = L"Неподдерживаемый формат файла данных. Выберите файл .DEV или .bin.";
			break;
		case kDataFileLayoutError:
			errorMessage = L"Файл данных повреждён или его размер не соответствует формату GP_DATA.";
			break;
		default:
			errorMessage = L"Не удалось проверить файл данных. Код ошибки: " + Convert::ToString(dataInfoResult);
			break;
		}
		errorMessage = DllErrorMessage(dataInfoResult, errorMessage);
		return dataInfoResult;
	}

	SONDE_ID_DBG selectedId = DecodeSondeId(dataSignature);
	const int transmitterCount = static_cast<int>(selectedId.N_Tx);
	const bool lwdFamily = selectedId.type_ == 2;
	const bool cartographLwdMode = selectedId.type_ == 3;
	const bool neuralToolSupported =
		transmitterCount == 4 && (lwdFamily || cartographLwdMode);
	if (!neuralToolSupported) {
		errorMessage = L"Нейросетевой расчёт поддерживает только приборы LWD с 4 передатчиками "
			L"и картограф в режиме LWD.";
		return 100;
	}

	std::ofstream fdbg("Debug_parse.txt");
	fdbg << std::fixed << std::setprecision(6);
	if (fdbg.is_open()) {
		fdbg << "######## METROLOGY (" << metroName << ") ########\n";
		fdbg << "sizeof(GP_DATA)      = " << sizeof(GP_DATA) << "\n";
		fdbg << "sizeof(GP_METROLOGY) = " << sizeof(GP_METROLOGY) << "\n";
		fdbg << "frame_header_size    = " << frameHeaderSize << "\n";
		fdbg << "frame_record_size    = " << (sizeof(GP_DATA) + frameHeaderSize) << "\n";
		fdbg << "metro.signature      = " << selectedMetro.signature << "\n";
		fdbg << "data.signature       = " << dataSignature << "\n";
		fdbg << "frames               = " << fileFrames << "\n\n";
	}

	ifstream dataInput(dataName.c_str(), ios::binary);
	if (!dataInput.is_open()) {
		errorMessage = L"Не удалось открыть файл данных:\n" + dataPath;
		return kDataFileOpenError;
	}

	const streamoff recordSize = static_cast<streamoff>(sizeof(GP_DATA) + frameHeaderSize);
	for (uint32_t n = 0; n < fileFrames; ++n) {
		GP_DATA currentFrame = {};
		const streamoff payloadOffset = static_cast<streamoff>(n) * recordSize + frameHeaderSize;
		dataInput.seekg(payloadOffset, ios::beg);
		dataInput.read(reinterpret_cast<char*>(&currentFrame), sizeof(GP_DATA));
		if (!dataInput) {
			errorMessage = L"Ошибка чтения кадра " + Convert::ToString(static_cast<unsigned int>(n)) + L" из файла данных.";
			return kDataFileLayoutError;
		}

		gp_data = currentFrame;
		if (fdbg.is_open() && (n < 3 || n == 300 || n == 600 || n == 900))
			DumpGpDataFrame(fdbg, static_cast<int>(n), gp_data);

		int result = get_Phase(&gp_data, &phase, 0);
		if (result != 0) {
			errorMessage = result == kFrameSignatureMismatch
				? L"Обнаружен кадр от другого прибора. Обработка остановлена."
				: L"DLL не смогла извлечь фазы из кадра. Код ошибки: " + Convert::ToString(result);
			errorMessage = DllErrorMessage(result, errorMessage);
			return result;
		}

		result = get_express_data(&gp_data, &phase_express, &ro_express, 0);
		if (result != 0) {
			errorMessage = result == kFrameSignatureMismatch
				? L"Обнаружен кадр от другого прибора. Обработка остановлена."
				: L"DLL не смогла извлечь данные кадра. Код ошибки: " + Convert::ToString(result);
			errorMessage = DllErrorMessage(result, errorMessage);
			return result;
		}

		result = get_condition(&gp_data, &condition, 0);
		if (result != 0) {
			errorMessage = result == kFrameSignatureMismatch
				? L"Обнаружен кадр от другого прибора. Обработка остановлена."
				: L"DLL не смогла извлечь состояние прибора. Код ошибки: " + Convert::ToString(result);
			errorMessage = DllErrorMessage(result, errorMessage);
			return result;
		}

		result = simmetry(&phase, &phase_smt, condition);
		if (result != 0) {
			errorMessage = DllErrorMessage(result, L"Не удалось выполнить симметризацию фаз.");
			return result;
		}

		result = calculate_Rho_AF(&phase_express, &ro_AF, ro_bh, D_bhole_nom, 0, 0, &service_AF);
		if (result != 0) {
			errorMessage = DllErrorMessage(result, L"Не удалось рассчитать УЭС/параметры зоны.");
			return result;
		}
		result = calculate_Rho_Doll_GR(&phase_smt, &Ro_3c);
		if (result != 0) {
			errorMessage = DllErrorMessage(result, L"Не удалось рассчитать УЭС по симметризованным фазам.");
			return result;
		}

		const float roP = ro_AF.Ro_p[_400_kGz];
		for (int tx = 0; tx < transmitterCount; ++tx) {
			charts[0]->Series[tx]->Points->AddXY(n, phase_express.Phase[_400_kGz][tx] * mG);
			charts[1]->Series[tx]->Points->AddXY(n, phase_express.Phase[_2000_kGz][tx] * mG);
			charts[2]->Series[tx]->Points->AddXY(n, ro_AF.Ro[_400_kGz][tx]);
			charts[3]->Series[tx]->Points->AddXY(n, ro_AF.Ro[_2000_kGz][tx]);
			charts[4]->Series[tx]->Points->AddXY(n, phase_express.Phase[_400_kGz][tx] * mG);
			charts[5]->Series[tx]->Points->AddXY(n, phase_express.Phase[_2000_kGz][tx] * mG);
			charts[6]->Series[tx]->Points->AddXY(n, ro_AF.Ro[_400_kGz][tx]);
			charts[7]->Series[tx]->Points->AddXY(n, ro_AF.Ro[_2000_kGz][tx]);
			charts[8]->Series[tx]->Points->AddXY(n, ro_AF.Ro[_400_kGz][tx]);
			charts[8]->Series[tx + 4]->Points->AddXY(n, ro_AF.Ro[_2000_kGz][tx]);
			charts[9]->Series[tx]->Points->AddXY(n, ro_express.Ro[_400_kGz][tx]);
			charts[9]->Series[tx + 4]->Points->AddXY(n, ro_AF.Ro[_400_kGz][tx]);
			charts[10]->Series[tx]->Points->AddXY(n, ro_express.Ro[_2000_kGz][tx]);
			charts[10]->Series[tx + 4]->Points->AddXY(n, ro_AF.Ro[_2000_kGz][tx]);
		}
		charts[4]->Series[4]->Points->AddXY(n, roP);
		charts[5]->Series[4]->Points->AddXY(n, roP);
		charts[6]->Series[4]->Points->AddXY(n, roP);
		charts[7]->Series[4]->Points->AddXY(n, roP);
		charts[8]->Series[8]->Points->AddXY(n, roP);

		if ((n % 50) == 0 || n + 1 == fileFrames) {
			statusLabel->Text = L"Обработка: " +
				Convert::ToString(static_cast<unsigned int>(n + 1)) + L" / " +
				Convert::ToString(static_cast<unsigned int>(fileFrames));
			Application::DoEvents();
		}
	}

	dataInput.close();
	if (fdbg.is_open()) fdbg.close();
	return 0;
}

public ref class TestController sealed {
private:
	Form^ form;
	cli::array<Chart^>^ charts;
	MenuStrip^ menuStrip;
	ToolStripStatusLabel^ statusLabel;
	String^ metrologyPath;
	String^ dataPath;
	bool processing;

	void UpdateSelectionStatus() {
		statusLabel->Text = L"Метрология: " + System::IO::Path::GetFileName(metrologyPath) +
			L" | Данные: " + System::IO::Path::GetFileName(dataPath) +
			L" | F5 — запустить";
	}

public:
	TestController(Form^ owner, cli::array<Chart^>^ chartList, String^ defaultMetrology, String^ defaultData) {
		form = owner;
		charts = chartList;
		metrologyPath = defaultMetrology;
		dataPath = defaultData;
		processing = false;

		menuStrip = gcnew MenuStrip();
		ToolStripMenuItem^ fileMenu = gcnew ToolStripMenuItem(L"Файл");
		ToolStripMenuItem^ selectMetrology = gcnew ToolStripMenuItem(L"Выбрать файл метрологии...");
		ToolStripMenuItem^ selectData = gcnew ToolStripMenuItem(L"Выбрать файл данных...");
		ToolStripMenuItem^ runTest = gcnew ToolStripMenuItem(L"Запустить тестирование");
		selectMetrology->ShortcutKeys = Keys::Control | Keys::M;
		selectData->ShortcutKeys = Keys::Control | Keys::D;
		runTest->ShortcutKeys = Keys::F5;
		selectMetrology->Click += gcnew EventHandler(this, &TestController::OnSelectMetrology);
		selectData->Click += gcnew EventHandler(this, &TestController::OnSelectData);
		runTest->Click += gcnew EventHandler(this, &TestController::OnRunTest);
		fileMenu->DropDownItems->Add(selectMetrology);
		fileMenu->DropDownItems->Add(selectData);
		fileMenu->DropDownItems->Add(gcnew ToolStripSeparator());
		fileMenu->DropDownItems->Add(runTest);
		menuStrip->Items->Add(fileMenu);
		form->MainMenuStrip = menuStrip;
		form->Controls->Add(menuStrip);

		StatusStrip^ statusStrip = gcnew StatusStrip();
		statusLabel = gcnew ToolStripStatusLabel();
		statusLabel->Spring = true;
		statusLabel->TextAlign = System::Drawing::ContentAlignment::MiddleLeft;
		statusStrip->Items->Add(statusLabel);
		form->Controls->Add(statusStrip);
		menuStrip->BringToFront();
		statusStrip->BringToFront();
		UpdateSelectionStatus();
	}

	void OnShown(Object^ sender, EventArgs^ e) {
		form->BeginInvoke(gcnew MethodInvoker(this, &TestController::RunSelectedFiles));
	}

	void OnSelectMetrology(Object^ sender, EventArgs^ e) {
		OpenFileDialog^ dialog = gcnew OpenFileDialog();
		dialog->Title = L"Выберите файл метрологии";
		dialog->Filter = L"Файл метрологии (*.bin)|*.bin|Все файлы (*.*)|*.*";
		dialog->FileName = metrologyPath;
		dialog->CheckFileExists = true;
		dialog->RestoreDirectory = true;
		if (dialog->ShowDialog(form) == DialogResult::OK) {
			metrologyPath = dialog->FileName;
			UpdateSelectionStatus();
		}
	}

	void OnSelectData(Object^ sender, EventArgs^ e) {
		OpenFileDialog^ dialog = gcnew OpenFileDialog();
		dialog->Title = L"Выберите файл данных";
		dialog->Filter = L"Данные прибора (*.DEV;*.bin)|*.DEV;*.dev;*.bin|DEV (*.DEV)|*.DEV;*.dev|BIN (*.bin)|*.bin|Все файлы (*.*)|*.*";
		dialog->FileName = dataPath;
		dialog->CheckFileExists = true;
		dialog->RestoreDirectory = true;
		if (dialog->ShowDialog(form) == DialogResult::OK) {
			dataPath = dialog->FileName;
			UpdateSelectionStatus();
		}
	}

	void OnRunTest(Object^ sender, EventArgs^ e) {
		RunSelectedFiles();
	}

	void RunSelectedFiles() {
		if (processing)
			return;

		processing = true;
		menuStrip->Enabled = false;
		form->UseWaitCursor = true;
		statusLabel->Text = L"Проверка выбранных файлов...";
		Application::DoEvents();

		String^ errorMessage = nullptr;
		int result = ProcessDataFiles(metrologyPath, dataPath, charts, statusLabel, errorMessage);

		form->UseWaitCursor = false;
		menuStrip->Enabled = true;
		processing = false;
		if (result == 0) {
			statusLabel->Text = L"Готово | Метрология: " + System::IO::Path::GetFileName(metrologyPath) +
				L" | Данные: " + System::IO::Path::GetFileName(dataPath);
		}
		else {
			statusLabel->Text = L"Ошибка обработки выбранных файлов";
			MessageBox::Show(form, errorMessage, L"SONDE — ошибка данных", MessageBoxButtons::OK, MessageBoxIcon::Error);
		}
	}
};

[STAThread]
int main()
{
#pragma region инициализация формы для отображения графиков

	Application::EnableVisualStyles();
	int width = 1200;

	Form^ form = gcnew Form;
	form->Text = " 'RAMON FILM' production";
	form->ClientSize = System::Drawing::Size(1200, 820);
	form->AutoScroll = false;
	Panel^ chartsPanel = gcnew Panel();
	chartsPanel->Dock = DockStyle::Fill;
	chartsPanel->AutoScroll = true;
	form->Controls->Add(chartsPanel);

	Chart^  chart1 = (gcnew Chart());
	chartsPanel->Controls->Add(chart1);
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
	chartsPanel->Controls->Add(chart2);
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
	chartsPanel->Controls->Add(chart3);
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
	chartsPanel->Controls->Add(chart4);
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
	chartsPanel->Controls->Add(chart5);
	chart5->Size = System::Drawing::Size(width, 500);
	chart5->Location = System::Drawing::Point(5, 2000);
	chart5->ChartAreas->Add("ChartArea1");
	chart5->Legends->Add("Legend1");
	chart5->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart5->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart5->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart5->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart5->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 5; i++) {
		chart5->Series->Add("series" + i);
		chart5->Series[i]->ChartType = SeriesChartType::Line;
		chart5->Series[i]->BorderWidth = 2;
	}
	chart5->Series[0]->LegendText = L"Ph T1 400";
	chart5->Series[1]->LegendText = L"Ph T2 400";
	chart5->Series[2]->LegendText = L"Ph T3 400";
	chart5->Series[3]->LegendText = L"Ph T4 400";
	chart5->Series[4]->LegendText = L"Ro_p";

	Chart^  chart6 = (gcnew Chart());
	chartsPanel->Controls->Add(chart6);
	chart6->Size = System::Drawing::Size(width, 500);
	chart6->Location = System::Drawing::Point(5, 2500);
	chart6->ChartAreas->Add("ChartArea1");
	chart6->Legends->Add("Legend1");
	chart6->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart6->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart6->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart6->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart6->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	for (int i = 0; i < 5; i++) {
		chart6->Series->Add("series" + i);
		chart6->Series[i]->ChartType = SeriesChartType::Line;
		chart6->Series[i]->BorderWidth = 2;
	}
	chart6->Series[0]->LegendText = L"Ph T1 2000";
	chart6->Series[1]->LegendText = L"Ph T2 2000";
	chart6->Series[2]->LegendText = L"Ph T3 2000";
	chart6->Series[3]->LegendText = L"Ph T4 2000";
	chart6->Series[4]->LegendText = L"Ro_p";

	Chart^  chart7 = (gcnew Chart());
	chartsPanel->Controls->Add(chart7);
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
	chartsPanel->Controls->Add(chart8);
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
	chartsPanel->Controls->Add(chart9);
	chart9->Size = System::Drawing::Size(form->ClientSize.Width - 10, 500);
	chart9->Location = System::Drawing::Point(5, 4000);
	chart9->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(
		System::Windows::Forms::AnchorStyles::Top |
		System::Windows::Forms::AnchorStyles::Left |
		System::Windows::Forms::AnchorStyles::Right);
	chart9->ChartAreas->Add("ChartArea1");
	chart9->Legends->Add("Legend1");
	chart9->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart9->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart9->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart9->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart9->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	cli::array<System::Drawing::Color>^ txColors = gcnew cli::array<System::Drawing::Color>(4);
	txColors[0] = System::Drawing::Color::Blue;
	txColors[1] = System::Drawing::Color::Red;
	txColors[2] = System::Drawing::Color::Green;
	txColors[3] = System::Drawing::Color::Orange;

	// Серии 0-3: Ro 400 кГц (T1-T4), сплошная линия.
	// Серии 4-7: Ro 2000 кГц (T1-T4), пунктирная линия тем же цветом.
	// Серия  8:   Ro_p на вторичной оси.
	for (int i = 0; i < 9; i++) {
		chart9->Series->Add("series" + i);
		chart9->Series[i]->ChartType = SeriesChartType::Line;
		chart9->Series[i]->BorderWidth = 2;
	}
	for (int Tx = 0; Tx < 4; Tx++) {
		chart9->Series[Tx]->Color = txColors[Tx];
		chart9->Series[Tx]->LegendText = L"Ro T" + (Tx + 1) + L" 400";
		chart9->Series[Tx]->BorderDashStyle = ChartDashStyle::Solid;
		chart9->Series[Tx + 4]->Color = txColors[Tx];
		chart9->Series[Tx + 4]->LegendText = L"Ro T" + (Tx + 1) + L" 2000";
		chart9->Series[Tx + 4]->BorderDashStyle = ChartDashStyle::Dash;
	}
	chart9->Series[8]->LegendText = L"Ro_p";
	chart9->Series[8]->Color = System::Drawing::Color::Black;
	chart9->Series[8]->BorderWidth = 3;
	chart9->Series[8]->YAxisType = AxisType::Secondary;
	chart9->ChartAreas["ChartArea1"]->AxisY2->Enabled = AxisEnabled::True;
	chart9->ChartAreas["ChartArea1"]->AxisY->Title = L"УЭС, Ом·м";
	chart9->ChartAreas["ChartArea1"]->AxisY2->Title = L"Ro_p, Ом·м";

	Chart^  chart10 = (gcnew Chart());
	chartsPanel->Controls->Add(chart10);
	chart10->Size = System::Drawing::Size(width, 500);
	chart10->Location = System::Drawing::Point(5, 4500);
	chart10->ChartAreas->Add("ChartArea1");
	chart10->Legends->Add("Legend1");
	chart10->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart10->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart10->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart10->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart10->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;


	// Серии 0-3: Ro FROM_SONDE 400 кГц (T1-T4), сплошная линия.
	// Серии 4-7: Ro RO_dFI 400 кГц (T1-T4), пунктирная линия тем же цветом.
	for (int i = 0; i < 8; i++) {
		chart10->Series->Add("series" + i);
		chart10->Series[i]->ChartType = SeriesChartType::Line;
		chart10->Series[i]->BorderWidth = 2;
	}
	for (int Tx = 0; Tx < 4; Tx++) {
		chart10->Series[Tx]->Color = txColors[Tx];
		chart10->Series[Tx]->LegendText = L"Ro T" + (Tx + 1) + L" 400 (sonde)";
		chart10->Series[Tx]->BorderDashStyle = ChartDashStyle::Solid;
		chart10->Series[Tx + 4]->Color = txColors[Tx];
		chart10->Series[Tx + 4]->LegendText = L"Ro T" + (Tx + 1) + L" 400 (RO_dFI)";
		chart10->Series[Tx + 4]->BorderDashStyle = ChartDashStyle::Dash;
	}
	chart10->ChartAreas["ChartArea1"]->AxisY->Title = L"УЭС, Ом·м";

	Chart^ chart11 = (gcnew Chart());
	chartsPanel->Controls->Add(chart11);
	chart11->Size = System::Drawing::Size(width, 500);
	chart11->Location = System::Drawing::Point(5, 5000);
	chart11->ChartAreas->Add("ChartArea1");
	chart11->Legends->Add("Legend1");
	chart11->ChartAreas["ChartArea1"]->CursorX->IsUserSelectionEnabled = true;
	chart11->ChartAreas["ChartArea1"]->CursorY->IsUserSelectionEnabled = true;
	chart11->ChartAreas["ChartArea1"]->CursorX->IsUserEnabled = true;
	chart11->ChartAreas["ChartArea1"]->CursorY->IsUserEnabled = true;
	chart11->ChartAreas["ChartArea1"]->CursorY->Interval = 0.0000001;

	for (int i = 0; i < 8; i++) {
		chart11->Series->Add("series" + i);
		chart11->Series[i]->ChartType = SeriesChartType::Line;
		chart11->Series[i]->BorderWidth = 2;
	}
	for (int Tx = 0; Tx < 4; Tx++) {
		chart11->Series[Tx]->Color = txColors[Tx];
		chart11->Series[Tx]->LegendText = L"Ro T" + (Tx + 1) + L" 2000 (sonde)";
		chart11->Series[Tx]->BorderDashStyle = ChartDashStyle::Solid;
		chart11->Series[Tx + 4]->Color = txColors[Tx];
		chart11->Series[Tx + 4]->LegendText = L"Ro T" + (Tx + 1) + L" 2000 (RO_dFI)";
		chart11->Series[Tx + 4]->BorderDashStyle = ChartDashStyle::Dash;
	}
	chart11->ChartAreas["ChartArea1"]->AxisY->Title = L"УЭС, Ом·м";




	
	// === Конфигурация chart5-8: новые графики FROM_SONDE и FROM_COMPUTER с Ro_p ===
	// chart5: симм. фазы 400 кГц из структуры (FROM_SONDE) + Ro_p на вторичной оси
	chart5->Series[0]->LegendText = L"Ph T1 400 (sonde)";
	chart5->Series[1]->LegendText = L"Ph T2 400 (sonde)";
	chart5->Series[2]->LegendText = L"Ph T3 400 (sonde)";
	chart5->Series[3]->LegendText = L"Ph T4 400 (sonde)";
	chart5->Series[4]->LegendText = L"Ro_p";
	chart5->Series[4]->YAxisType = AxisType::Secondary;
	chart5->Series[4]->BorderWidth = 3;
	chart5->ChartAreas["ChartArea1"]->AxisY2->Enabled = AxisEnabled::True;
	chart5->ChartAreas["ChartArea1"]->AxisY->Title = L"Фаза, мГрад";
	chart5->ChartAreas["ChartArea1"]->AxisY2->Title = L"Ro_p, Ом·м";

	// chart6: симм. фазы 2000 кГц из структуры (FROM_SONDE) + Ro_p на вторичной оси
	chart6->Series[0]->LegendText = L"Ph T1 2000 (sonde)";
	chart6->Series[1]->LegendText = L"Ph T2 2000 (sonde)";
	chart6->Series[2]->LegendText = L"Ph T3 2000 (sonde)";
	chart6->Series[3]->LegendText = L"Ph T4 2000 (sonde)";
	chart6->Series[4]->LegendText = L"Ro_p";
	chart6->Series[4]->YAxisType = AxisType::Secondary;
	chart6->Series[4]->BorderWidth = 3;
	chart6->ChartAreas["ChartArea1"]->AxisY2->Enabled = AxisEnabled::True;
	chart6->ChartAreas["ChartArea1"]->AxisY->Title = L"Фаза, мГрад";
	chart6->ChartAreas["ChartArea1"]->AxisY2->Title = L"Ro_p, Ом·м";

	// chart7: УЭС 400 кГц из структуры (FROM_SONDE) + Ro_p
	chart7->Series[0]->LegendText = L"Ro T1 400";
	chart7->Series[1]->LegendText = L"Ro T2 400";
	chart7->Series[2]->LegendText = L"Ro T3 400";
	chart7->Series[3]->LegendText = L"Ro T4 400";
	chart7->Series[4]->LegendText = L"Ro_p";
	chart7->Series[4]->YAxisType = AxisType::Secondary;
	chart7->Series[4]->BorderWidth = 3;
	chart7->ChartAreas["ChartArea1"]->AxisY2->Enabled = AxisEnabled::True;
	chart7->ChartAreas["ChartArea1"]->AxisY->Title = L"УЭС, Ом·м";
	chart7->ChartAreas["ChartArea1"]->AxisY2->Title = L"Ro_p, Ом·м";

	// chart8: УЭС 2000 кГц из структуры (FROM_SONDE) + Ro_p
	chart8->Series[0]->LegendText = L"Ro T1 2000";
	chart8->Series[1]->LegendText = L"Ro T2 2000";
	chart8->Series[2]->LegendText = L"Ro T3 2000";
	chart8->Series[3]->LegendText = L"Ro T4 2000";
	chart8->Series[4]->LegendText = L"Ro_p";
	chart8->Series[4]->YAxisType = AxisType::Secondary;
	chart8->Series[4]->BorderWidth = 3;
	chart8->ChartAreas["ChartArea1"]->AxisY2->Enabled = AxisEnabled::True;
	chart8->ChartAreas["ChartArea1"]->AxisY->Title = L"УЭС, Ом·м";
	chart8->ChartAreas["ChartArea1"]->AxisY2->Title = L"Ro_p, Ом·м";

#pragma endregion 

#pragma region library DLL
	                             
	if (!LoadSondeLibrary()) {
		cout << "SONDE DLL was not loaded. Put a compatible SONDE_DLL_1.0.dll next to SONDE_1.0.exe or update the DLL path." << endl;
	}
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
	if (!get_Phase)
		cout << "Unable to find the function 'get_Phase' " << endl;
	else cout << "get_Phase is  ok" << endl;

	get_condition = (Get_condition)GetProcAddress(SONDE_3_C, "get_condition");
	if (!get_condition)
		cout << "Unable to find the function 'get_condition' " << endl;
	else cout << "get_condition is  ok" << endl;

	simmetry = (Simmetry)GetProcAddress(SONDE_3_C, "simmetry");
	if (!simmetry)
		cout << "Unable to find the function 'simmetry' " << endl;
	else cout << "simmetry is  ok" << endl;

	calculate_Rho_AF = (Calculate_Rho_AF)GetProcAddress(SONDE_3_C, "calculate_Rho_AF");
	if (!calculate_Rho_AF)
		cout << "Unable to find the function 'calculate_Rho_AF' " << endl;
	else cout << "calculate_Rho_AF is  ok" << endl;

	ph_shift_smt_ph = (Ph_shift_smt_ph)GetProcAddress(SONDE_3_C, "ph_shift_smt_ph");
	if (!ph_shift_smt_ph)
		cout << "Unable to find the function 'ph_shift_smt_ph' " << endl;
	else cout << "ph_shift_smt_ph is  ok" << endl;

	ph_shift_smt_ro = (Ph_shift_smt_ro)GetProcAddress(SONDE_3_C, "ph_shift_smt_ro");
	if (!ph_shift_smt_ro)
		cout << "Unable to find the function 'ph_shift_smt_ro' " << endl;
	else cout << "ph_shift_smt_ro is  ok" << endl;

	ro_corr_ref_point = (Ro_corr_ref_point)GetProcAddress(SONDE_3_C, "ro_corr_ref_point");
	if (!ro_corr_ref_point)
		cout << "Unable to find the function 'ro_corr_ref_point' " << endl;
	else cout << "ro_corr_ref_point is  ok" << endl;

	ph_smt_ro = (Ph_smt_ro)GetProcAddress(SONDE_3_C, "ph_smt_ro");
	if (!ph_smt_ro)
		cout << "Unable to find the function 'ph_smt_ph' " << endl;
	else cout << "ph_smt_ro is  ok" << endl;

	debug_mode = (Debug_mode)GetProcAddress(SONDE_3_C, "debug_mode");
	if (!debug_mode)
		cout << "Unable to find the function 'debug_mode' " << endl;
	else cout << "debug_mode is  ok" << endl;

	calculate_Rho_Doll_GR = (Calculate_Rho_Doll_GR)GetProcAddress(SONDE_3_C, "calculate_Rho_Doll_GR");
	if (!calculate_Rho_Doll_GR)
		cout << "Unable to find the function 'calculate_Rho_Doll_GR' " << endl;
	else cout << "calculate_Rho_Doll_GR is  ok" << endl;

	ph_smt_zp = (Ph_smt_zp)GetProcAddress(SONDE_3_C, "ph_smt_zp");
	if (!ph_smt_zp)
		cout << "Unable to find the function 'ph_smt_zp' " << endl;
	else cout << "ph_smt_zp is  ok" << endl;

	get_data_file_info = (Get_data_file_info)GetProcAddress(SONDE_3_C, "get_data_file_info");
	if (!get_data_file_info)
		cout << "Unable to find the function 'get_data_file_info' " << endl;
	else cout << "get_data_file_info is  ok" << endl;

	get_sonde_last_error = (Get_sonde_last_error)GetProcAddress(SONDE_3_C, "sonde_get_last_error");
	if (!get_sonde_last_error)
		cout << "Unable to find the function 'sonde_get_last_error' " << endl;
	else cout << "sonde_get_last_error is  ok" << endl;

#pragma endregion 

	if (!RequiredDllFunctionsLoaded()) {
		cout << "SONDE DLL is loaded, but it does not contain all functions required by this test project." << endl;
		cout << "Use the updated SONDE_DLL_1.0.dll from the DLL project with the same platform as this executable." << endl;
		FreeLibrary(SONDE_3_C);
		return 1;
	}

	debug_mode(1);

	cli::array<Chart^>^ charts = gcnew cli::array<Chart^>(11);
	charts[0] = chart1;
	charts[1] = chart2;
	charts[2] = chart3;
	charts[3] = chart4;
	charts[4] = chart5;
	charts[5] = chart6;
	charts[6] = chart7;
	charts[7] = chart8;
	charts[8] = chart9;
	charts[9] = chart10;
	charts[10] = chart11;

	TestController^ controller = gcnew TestController(
		form,
		charts,
		gcnew String(kDefaultMetroName),
		gcnew String(kDefaultDataName));
	form->Shown += gcnew EventHandler(controller, &TestController::OnShown);

	Application::Run(form);
	FreeLibrary(SONDE_3_C);
	return 0;
}
