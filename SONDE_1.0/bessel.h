#pragma once
#include <complex>
#include "variable.h"

//const double PI = 3.1415927410125732;
const double eiler = 0.5772156649015328606065120900824024310421;
const double e = 2.7182818284590452354;
//const double eps0 = 8.85*1e-12;
//const double mu0 = 4 * PI*1e-7;

struct BESSEL_PALLETE {
	double FxF[100]; //произведение факториалов
	double FxG[100];//произведение факториала на гамма функцию
	double HxFxF[100];//произведение гармонического ряда на квадрат факториала для К0
	double HxH1FxG[100];//произведение гармонических рядов к и к+1 факториала и гамма функции для К1
	double HankelK0[100];
	double HankelK1[100];
};
__declspec(selectany) BESSEL_PALLETE pallete;

double psi(int m);

double H_(int m);

double fact(int n);

double gamma05(int n);

void FxF(int n);
//произведение факториала на гамма функцию
void FxG(int n);
//произведение гармонического ряда на квадрат факториала для К0
void HxFxF(int n);
//произведение гармонических рядов к и к+1 факториала и гамма функции для К1
void HxH1FxG(int n);
// символ Ханкеля для К0 большого
void Hankel_K0(int n);
// символ Ханкеля для К1 большого
void Hankel_K1(int n);

//перегрузка функций Бесселя I0 I1 K0 K1 с палетками и без
std::complex<double> I0(std::complex<double> z);

std::complex<double> I1(std::complex<double> z);

std::complex<double> K0_small(std::complex<double> z);

std::complex<double> K1_small(std::complex<double> z);

std::complex<double> K0_big(std::complex<double> z);

std::complex<double> K1_big(std::complex<double> z);

std::complex<double> K0(std::complex<double> z);

std::complex<double> K1(std::complex<double> z);

void create_bessel_pallete(void);

float Vzz_inf_cyl(SONDE_PARAM param, float Ro_p, float Ro_zp, float rzp);