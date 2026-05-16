#include "windows.h"
#include "stdafx.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <complex> 
#include <string> 
#include "function.h"
using namespace std;

double  Ro_0 = 0.01;  //минимальное значение ус для расчета
double  Ro_max = 10000;  //максимальное значение ус для расчета
double epsilon_ABS = 0.0000005; // точность ,до которой возможны измерения амплитуды 
double epsilon_ARG = 0.0000005; // точность ,до которой возможны измерения фазы
double delta;
double  ro_0 = Ro_0;
double  ro_max = Ro_max;

uint32_t	mantTable[2048];
uint32_t	expTable[64];
uint16_t	offsTable[64];
uint16_t	baseTable[512];
uint8_t		shiftTable[512];

static unsigned int convertmantissa(unsigned int i)
{
	uint32_t m = i << 13;		// zero pad mantissa bits
	uint32_t e = 0; 			// zero exponent

	while (!(m & 0x00800000))	 // while not normalized
	{
		e -= 0x00800000;		// decrement exponent (1<<23)
		m <<= 1;				// shift mantissa
	}
	m &= ~0x00800000;			// clear leading 1 bit
	e += 0x38800000;			// adjust bias ((127-14)<<23)

	return m | e;				// return combined number
}

static void	initTables()
{
	static	bool	inited = false;
	int	i;

	if (inited)
		return;

	inited = true;
	mantTable[0] = 0;

	for (i = 1; i < 1024; i++)
		mantTable[i] = convertmantissa(i);

	for (i = 1024; i < 2048; i++)
		mantTable[i] = 0x38000000 + ((i - 1024) << 13);

	expTable[0] = 0;
	expTable[32] = 0x80000000;

	for (i = 1; i <= 30; i++)
		expTable[i] = i << 23;

	for (int i = 33; i < 63; i++)
		expTable[i] = 0x80000000 + ((i - 32) << 23);

	expTable[31] = 0x47800000;
	expTable[63] = 0xC7800000;

	for (i = 1; i < 64; i++)
		offsTable[i] = 1024;

	offsTable[0] = 0;
	offsTable[32] = 0;

	for (i = 0; i < 256; i++)
	{
		int	e = i - 127;

		if (e < -24)			// Very small numbers map to zero
		{
			baseTable[i | 0x000] = 0x0000;
			baseTable[i | 0x100] = 0x8000;
			shiftTable[i | 0x000] = 24;
			shiftTable[i | 0x100] = 24;
		}
		else if (e < -14)		// Small numbers map to denorms
		{
			baseTable[i | 0x000] = static_cast<uint16_t>(0x0400 >> (-e - 14));
			baseTable[i | 0x100] = static_cast<uint16_t>((0x0400 >> (-e - 14)) | 0x8000);
			shiftTable[i | 0x000] = -e - 1;
			shiftTable[i | 0x100] = -e - 1;
		}
		else if (e <= 15)		// Normal numbers just lose precision
		{
			baseTable[i | 0x000] = static_cast<uint16_t>((e + 15) << 10);
			baseTable[i | 0x100] = static_cast<uint16_t>(((e + 15) << 10) | 0x8000);
			shiftTable[i | 0x000] = 13;
			shiftTable[i | 0x100] = 13;
		}
		else if (e < 128)		// Large numbers map to Infinity
		{
			baseTable[i | 0x000] = 0x7C00;
			baseTable[i | 0x100] = 0xFC00;
			shiftTable[i | 0x000] = 24;
			shiftTable[i | 0x100] = 24;
		}
		else					// Infinity and NaN's stay Infinity and NaN's
		{
			baseTable[i | 0x000] = 0x7C00;
			baseTable[i | 0x100] = 0xFC00;
			shiftTable[i | 0x000] = 13;
			shiftTable[i | 0x100] = 13;
		}
	}
}

void halfToFloat(const uint16_t * half, uint32_t * f, int count)
{
	initTables();

	for (int i = 0; i < count; i++)
	{
		uint16_t	h = half[i];

		f[i] = mantTable[offsTable[h >> 10] + (h & 0x3ff)] + expTable[h >> 10];
	}
}

void floatToHalf(const uint32_t * ft, uint16_t * half, int count)
{
	initTables();

	for (int i = 0; i < count; i++)
	{
		uint32_t f = ft[i];

		half[i] = static_cast<uint16_t>(baseTable[(f >> 23) & 0x1ff] + ((f & 0x007fffff) >> shiftTable[(f >> 23) & 0x1ff]));
	}
}


GP_DATA READ_BLOK_GP_DATA_DEV(ifstream &fin, int n) {
	int pre = 11;
	GP_DATA gp_data;
	uint8_t *buff = new uint8_t[32];
	fin.seekg(n * (sizeof(GP_DATA) + pre));
	fin.read((char*)buff, pre);
	fin.read((char*)&gp_data, sizeof(GP_DATA));
	//fin.read((char*)buff, post);
	return gp_data;
}




float dFI(SONDE_PARAM param, float Ro) {
	float PI = 3.1415927410125732f;
	float eps0 = 8.85f * 1e-12f;
	float mu0 = 4.0f * PI * 1e-7f;
	float omega = static_cast<float>(2 * PI * param.f);
	float sigma = 1.0f / Ro;
	float eps1 = static_cast<float>(108.5 * pow(sigma, 0.35) + 5);
	
	complex<float> j(0.0f, 1.0f);
	complex<float> ik = j * sqrt(j * omega * mu0 * (sigma - complex<float>(0.0f, omega * eps0 * eps1)));
	complex<float> ZC1 = exp(ik*(param.L1 - param.L2)) * static_cast<float>(pow((param.L2 / param.L1), 3)) * ((1.0f - ik * param.L1) / (1.0f - ik * param.L2));
	return -arg(ZC1);
}
//УЭС от фазы по золотому сечению
float RO_dFI(SONDE_PARAM param, double dfi) {
	float Ro0;
	float epsilon_ARG = 0.0000005f; // точность  фазы и амплитуды 
	float  Ro_0 = 0.01f, Ro_max = 7000.0f;  //мин. и мах. значение ус для расчета
	float delta;
	float  ro_0 = Ro_0;
	float  ro_max = Ro_max;
	//  разность фаз 
	do {
		float X1 = ro_0 + 0.382f*(ro_max - ro_0);
		float X2 = ro_max - 0.382f*(ro_max - ro_0);
		float A = static_cast<float>(dfi - dFI(param, X1));
		float B = static_cast<float>(dfi - dFI(param, X2));
		if (fabs(A) > fabs(B)) { ro_0 = X1; }
		else { ro_max = X2; }
		if (A == 0 || B == 0) { delta = 0; }
		else { delta = fabs(A - B); }
	} while (delta > epsilon_ARG);

	Ro0 = (ro_0 + ro_max) / 2.0f;
	ro_0 = Ro_0;
	ro_max = Ro_max;
	return  Ro0;
}


float Vzz_inf_cyl_Doll(SONDE_PARAM param, float Ro_p, float sigma_zp, float r_zp) {
	float Vzz_inf = dFI( param,  Ro_p);
	//float sigma_zp = 1 / Ro_zp;
	float omega = static_cast<float>(2 * PI * param.f);
	float L = (param.L1 + param.L2) / 2.0f; float dL = param.L2 - param.L1;
	float P12 = static_cast<float>((omega*mu0*param.L1*param.L1*param.L1) / 4.0);
	float P22 = static_cast<float>((omega*mu0*param.L2*param.L2*param.L2) / 4.0);
	
	float Vzz_cyl_Doll = 0.0f;
	float  dz = 0.01f, dr = 0.01f;
	for (float r = 0.0f; r <= r_zp + 0.00f; r += 0.01f) {
		float QL1 = 0.0f; float QL2 = 0.0f;
		for (float z = -5.0f; z < 5.0f; z += dz) {
			float r00 = static_cast<float>(sqrt(pow((z - L), 2) + pow(r, 2)));
			float r01 = static_cast<float>(sqrt(pow((z - dL / 2.0f), 2) + pow(r, 2)));
			float r11 = static_cast<float>(sqrt(pow((z + dL / 2.0f), 2) + pow(r, 2)));
			QL1 += static_cast<float>((P12*(pow(r, 3) * dr * dz)) / pow(r00*r01, 3));
			QL2 += static_cast<float>((P22*(pow(r, 3) * dr * dz)) / pow(r00*r11, 3));
		}
		complex<float> j(0.0f, 1.0f);
		Vzz_cyl_Doll += arg((1.0f + j * QL2*sigma_zp) / (1.0f + j * QL1*sigma_zp));
	}
	return Vzz_cyl_Doll + Vzz_inf;
}

