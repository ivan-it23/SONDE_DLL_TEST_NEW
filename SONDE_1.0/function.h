#pragma once
#include "variable.h"

#define float16_t unsigned short

void halfToFloat(const uint16_t * half, uint32_t * f, int count);
void floatToHalf(const uint32_t * ft, uint16_t * half, int count);

GP_DATA READ_BLOK_GP_DATA_DEV(ifstream &fin, int n);
// Декодирует сигнатуру в идентификатор прибора и размер структуры кадра
// (struct_size: 240 для старой прошивки, 320 для новой с амплитудным каналом).
ID get_sonde_id(uint32_t signature);
float RO_dFI(SONDE_PARAM param, double dfi);
float dFI(SONDE_PARAM param, float Ro);
float Vzz_inf_cyl_Doll(SONDE_PARAM param, float Ro_p, float Ro_zp, float r_zp);
