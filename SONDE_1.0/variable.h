#pragma once
#include "windows.h"
#include "stdafx.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <complex> 
#include <string> 
#include <vector>
#include <cstddef>
//#include "bessel.h"
#define int16_t short
#define int32_t long
#define uint32_t unsigned long
#define Complex complex<long int>
#define uint16_t unsigned short
#define float16_t unsigned short


using namespace std;

const double PI = 3.1415927410125732;
const double eps0 = 8.85*1e-12;
const double mu0 = 4 * PI*1e-7;
const float mV = static_cast<float>(2500 / pow(2, 28));
const float mG = static_cast<float>(1000 * 180 / PI);
const float sG = static_cast<float>(200 * 180 / PI);//половина сантиградуса = 0.005 градуса
const float Grad = static_cast<float>(180 / PI);


// Сигнатурные коды типов приборов 
#define LWD_3Tx_            231
#define LWD_4Tx_            241
#define LWD_4Tx_NEW_        242
#define AUTONOM_5Tx_        151
#define AUTONOM_5Tx_SDR_    152
#define CARTOGRAPH_LWD_4Tx_ 349

enum FREQ {
	_400_kGz,
	_2000_kGz,
};

enum T_SMT {
	T1, T2, T3, T4,T5
};

#pragma pack(push, 1)

// Исторические структуры фазового канала. Сохранены для совместимости, но
// актуальный обмен с DLL идёт через CAL_SIGNAL/RHO (фаза + амплитуда).
struct PHASE {
	float Phase[2][5];
	float Depth;
};

struct Ro {
	float Ro[2][5];
	float Ro_p[2];
	float R_zp[2];
	float Ro_zp[2];
	float Depth;
};

// Калиброванный сигнал зонда: фазовый (phase, рад) и амплитудный (att_dB, дБ) каналы.
struct CAL_SIGNAL {
	float phase[2][5];
	float att_dB[2][5];
	float Depth;
};

// УЭС по фазовому и амплитудному каналам + параметры зоны проникновения.
struct RHO {
	float rho_ph[2][5];
	float rho_att[2][5];
	float rho_p[2];
	float R_zp[2];
	float rho_zp[2];
	float Depth;
};

// Идентификатор прибора со размером структуры кадра (декодируется из сигнатуры).
struct ID {
	uint32_t struct_size;
	uint32_t type_;
	uint32_t N_Tx;
	uint32_t mod;
	uint32_t number;
	uint32_t type;
};

struct ZP {
	float R_zp;
	float Ro_zp;
	float Ro_p;
	float tf;
};


struct SERVICE {
	float delta_percent_min[2];
	float delta_percent_start[2];
};

struct GP_DATA {
	uint32_t signature;
	uint32_t condition;
	uint32_t frame;
	float temperature;
	float rho_smt[2][5];// УЭС, рассчитанные на контроллере [400, 2000][T1-T5]
	float phase_smt[2][5];// симметризованные фазы [400, 2000][T1-T5]
	float AM_RX_1[2][5];// амплитуды на первом приемнике [400, 2000][T1-T5]
	float ZERO_AM_RX_1[2]; // амплитуды на первом приемнике [400, 2000] при молчащих передатчиках
	float AM_RX_2[2][5];// амплитуды на втором приемнике[400, 2000][T1 - T5]
	float ZERO_AM_RX_2[2];// амплитуды на втором приемнике [400, 2000] при молчащих передатчиках
	float DELTA_PH[2][5];// сырая разница фаз [400, 2000][T1 - T5]
	float ZERO_dPH[2];// разница фаз молчащих передатчиков;
	float rho_att_smt[2][5];// амплитудные УЭС, рассчитанные на контроллере [400, 2000][T1-T5]
	float att_smt_dB[2][5];// симметризованные децибельные затухания [400, 2000][T1-T5]
};

struct	GP_METROLOGY {
	uint32_t signature;
	uint32_t serial;
	uint16_t L1[5];//
	uint16_t L2[5];//
	uint16_t F[2];//
	int16_t Air_ph[2][5];// фазовые нули воздуха (было Air_zz)
	int16_t min_amp[2][5];// минимальные амплитуды (было Air_zz_amt)
	uint32_t D_sonde_mm;
	uint32_t work_type;
	uint32_t Rx_Position;
	float Air_att_dB[2][5];// амплитудные нули воздуха, дБ
	uint16_t service[58];//резерв до 240 байт
};

struct SONDE_PARAM {
	float L1;
	float L2;
	float f;
	float M;
	float log_M;
	float D_sonde_m;
};

struct K_B {
	float k;
	float b;
};


struct INF_CYL {
	complex <float> sonde[2][5];
};

#pragma pack(pop)

static_assert(sizeof(GP_DATA) == 320, "GP_DATA layout must match SONDE_DLL_NEW");
static_assert(offsetof(GP_DATA, rho_att_smt) == 240, "GP_DATA.rho_att_smt offset mismatch");
static_assert(offsetof(GP_DATA, att_smt_dB) == 280, "GP_DATA.att_smt_dB offset mismatch");
static_assert(sizeof(GP_METROLOGY) == 240, "GP_METROLOGY layout must match SONDE_DLL_NEW");
static_assert(offsetof(GP_METROLOGY, L1) == 8, "GP_METROLOGY.L1 offset mismatch");
static_assert(offsetof(GP_METROLOGY, L2) == 18, "GP_METROLOGY.L2 offset mismatch");
static_assert(offsetof(GP_METROLOGY, F) == 28, "GP_METROLOGY.F offset mismatch");
static_assert(offsetof(GP_METROLOGY, Air_ph) == 32, "GP_METROLOGY.Air_ph offset mismatch");
static_assert(offsetof(GP_METROLOGY, min_amp) == 52, "GP_METROLOGY.min_amp offset mismatch");
static_assert(offsetof(GP_METROLOGY, D_sonde_mm) == 72, "GP_METROLOGY.D_sonde_mm offset mismatch");
static_assert(offsetof(GP_METROLOGY, work_type) == 76, "GP_METROLOGY.work_type offset mismatch");
static_assert(offsetof(GP_METROLOGY, Rx_Position) == 80, "GP_METROLOGY.Rx_Position offset mismatch");
static_assert(offsetof(GP_METROLOGY, Air_att_dB) == 84, "GP_METROLOGY.Air_att_dB offset mismatch");
static_assert(offsetof(GP_METROLOGY, service) == 124, "GP_METROLOGY.service offset mismatch");
