#include "stdafx.h"
#include "bessel.h"
#include <complex>
#include <cmath>

int inf_I = 50;
int inf_K_small = 50;
int inf_K_big = 14;
//const double PI = 3.1415927410125732;

double psi(int m) {
	if (m == 0) return 0.0;
	double summ = -eiler;
	for (int i = 1; i < m; i++) {
		summ += 1.0 / i;
	}
	return summ;
}

double H_(int m) {
	if (m == 0) return 0.0;
	double summ = 0;
	for (int i = 1; i <= m; i++) {
		summ += 1.0 / i;
	}
	return summ;
}

double fact(int n) {
	if (n == 0)return 1;
	double summ = 1;
	for (int i = 1; i <= n; i++) {
		summ *= i;
	}
	return summ;
}

double gamma05(int n) {
	if (n < 0) return -2.0*sqrt(PI);
	double g = sqrt(PI);
	for (double i = 0.5; i < n; i++) {
		g *= i;
	}
	return g;
}

void FxF(int n) {
	pallete.FxF[0] = 1;
	double fxf = 1;
	for (int k = 1; k <= n; k++) {
		fxf *= k * k;
		pallete.FxF[k] = fxf;
	}
}
//произведение факториала на гамма функцию
void FxG(int n) {
	pallete.FxG[0] = 1;
	double fxg = 1;
	for (int k = 1; k <= 100; k++) {
		fxg *= k * k + k;
		pallete.FxG[k] = fxg;
	}
}
//произведение гармонического ряда на квадрат факториала для К0
void HxFxF(int n) {
	pallete.HxFxF[0] = 1;
	double fxf = 1;
	double H = 0;
	for (int k = 1; k <= 100; k++) {
		fxf *= k * k;
		H += 1.0 / k;
		pallete.HxFxF[k] = H / fxf;
	}
}
//произведение гармонических рядов к и к+1 факториала и гамма функции для К1
void HxH1FxG(int n) {
	pallete.HxH1FxG[0] = 1;
	double fxg = 1;
	double H = 0;
	double H1 = 0;
	for (int k = 1; k <= 100; k++) {
		fxg *= k * k + k;
		H += 1.0 / k;
		H1 = H + 1.0 / (k + 1);
		pallete.HxH1FxG[k] = (H + H1) / fxg;
	}
}
// символ Ханкеля для К0 большого
void Hankel_K0(int n) {
	for (int k = 0; k < n; k++) {
		pallete.HankelK0[k] = pow(PI*fact(k), -1)*pow(-1, k)*gamma05(k)*gamma05(k);
	}
}
// символ Ханкеля для К1 большого
void Hankel_K1(int n) {
	for (int k = 0; k < n; k++) {
		pallete.HankelK1[k] = -pow(PI*fact(k), -1)*pow(-1, k)*gamma05(k + 1)*gamma05(k - 1);
	}
}

//функции Бесселя I0 I1 K0 K1 
double fxf[50] = { 1,1,4,36,576,14400,518400,2.54016e+07,1.6257e+09,1.31682e+11,1.31682e+13,1.59335e+15,2.29443e+17,3.87758e+19,7.60005e+21,1.71001e+24,4.37763e+26,1.26514e+29,4.09904e+31,1.47975e+34,5.91901e+36,2.61028e+39,1.26338e+42,6.68327e+44,3.84956e+47,2.40598e+50,1.62644e+53,1.18567e+56,9.29569e+58,7.81768e+61,7.03591e+64,6.76151e+67,6.92378e+70,7.54e+73,8.71624e+76,1.06774e+80,1.38379e+83,1.89441e+86,2.73553e+89,4.16074e+92,6.65718e+95,1.11907e+99,1.97404e+102,3.65e+105,7.06641e+108,1.43095e+112,3.02789e+115,6.6886e+118,1.54105e+122,3.70007e+125 };

std::complex<double> I0(std::complex<double> z) {

	std::complex<double> summ = 1;
	for (int k = 1; k < inf_I; k++) {
		summ += pow(z / 2.0, 2 * k) / (pallete.FxF[k]);
	}
	return summ;
}

std::complex<double> I1(std::complex<double> z) {
	std::complex<double> summ = 0.5*z;
	for (int k = 1; k < inf_I; k++) {
		summ += pow(0.5*z, (2 * k) + 1) / (pallete.FxG[k]);
	}
	return summ;
}

std::complex<double> K0_small(std::complex<double> z) {
	std::complex<double> summ = 0;
	summ -= (log(0.5*z) + eiler) * I0(z);
	for (int k = 1; k < inf_K_small; k++) {
		summ += pow(0.5*z, 2 * k)*pallete.HxFxF[k];
	}
	return summ;
}

std::complex<double> K1_small(std::complex<double> z) {
	std::complex<double> summ = 0.5*z;
	double FxG = 1;
	for (int k = 1; k < inf_K_small; k++) {
		FxG *= k * k + k;
		summ += pow(0.5*z, 2 * k + 1)*pallete.HxH1FxG[k];
	}
	summ *= -0.5;
	summ += +1.0 / z + (log(0.5*z) + eiler) * I1(z);
	return summ;
}

std::complex<double> K0_big(std::complex<double> z) {
	std::complex<double> summ = 0;
	for (int k = 0; k < inf_K_big; k++) {
		summ += pallete.HankelK0[k] * pow(2.0*z, -k);
	}
	summ *= sqrt(0.5*PI / z)*exp(-z);
	return summ;
}

std::complex<double> K1_big(std::complex<double> z) {
	std::complex<double> summ = 0;
	for (int k = 0; k < inf_K_big; k++) {
		summ += pallete.HankelK1[k] * pow(2.0*z, -k);
	}
	summ *= sqrt(0.5*PI / z)*exp(-z);
	return summ;
}

std::complex<double> K0(std::complex<double> z) {
	if (abs(z) < 10)return K0_small(z);
	return K0_big(z);
}

std::complex<double> K1(std::complex<double> z) {
	if (abs(z) < 10)return K1_small(z);
	return K1_big(z);
}

void create_bessel_pallete() {
	FxF(90);
	FxG(90);
	HxFxF(90);
	HxH1FxG(90);
	Hankel_K0(50);
	Hankel_K1(50);
}

float Vzz_inf_cyl(SONDE_PARAM param, float Ro_p, float Ro_zp, float rzp) {
	float Vzz = 0.0f;
	double r0 = (double)rzp;
	double W = 2.0*PI*param.f;
	double dkz1 = 1e-4;
	double dkz2;
	if (rzp <= 0.1 && param.L1 > 0.8) dkz2 = 1.002711275;//2^1/128
	if(rzp <= 0.1 && param.L1 <= 0.8) dkz2 = 1.005429901128;//2^1/128
	if (rzp > 0.1 && rzp <= 0.2) dkz2 = 1.010889286;//2^1/64
	if (rzp > 0.2 && rzp <= 0.4) dkz2 = 1.0218971486;//2^1/32
	if (rzp > 0.4 ) dkz2 = 1.0442737824;//2^1/16
	double sigma1 = 1.0 / Ro_zp;
	double sigma2 = 1.0 / Ro_p;
	double eps1 = 108.5 * pow(sigma1, 0.35) + 5;
	double eps2 = 108.5 * pow(sigma2, 0.35) + 5;
	std::complex<double> j = { 0.0,1.0 };
	std::complex<double> k1 = (j*W*mu0*(sigma1 - j * W*eps0*eps1));
	std::complex<double> k2 = (j*W*mu0*(sigma2 - j * W*eps0*eps2));
	std::complex<double> inf_L1 = exp(j*sqrt(k1)*(double)param.L1)*(1.0 - j * sqrt(k1)*(double)param.L1) / pow((double)param.L1, 3);
	std::complex<double> inf_L2 = exp(j*sqrt(k1)*(double)param.L2)*(1.0 - j * sqrt(k1)*(double)param.L2) / pow((double)param.L2, 3);

	std::complex<double> integral_L1 = { 0.0,0.0 };
	std::complex<double> integral_L2 = { 0.0,0.0 };

	for (double kz = dkz1; kz < 0.1; kz += dkz1) {

		std::complex<double> x1 = sqrt(pow(kz, 2) - k1);
		std::complex<double> x2 = sqrt(pow(kz, 2) - k2);
		std::complex<double> part_b = pow(x1, 2)*((x1*K1(x2*r0)*K0(x1*r0) - x2 * K0(x2*r0)*K1(x1*r0)) /
			(x1*K1(x2*r0)*I0(x1*r0) + x2 * K0(x2*r0)*I1(x1*r0)));
		integral_L1 += cos(kz*(double)param.L1)*part_b;
		integral_L2 += cos(kz*(double)param.L2)*part_b;
		// iteration1++;
	}
	integral_L1 *= dkz1;
	integral_L2 *= dkz1;

	double dkz = dkz1;
	double kz = 0.1;
	while (kz < 80) {
		kz += dkz;
		dkz *= dkz2;
		std::complex<double> x1 = sqrt(pow(kz, 2) - k1);
		std::complex<double> x2 = sqrt(pow(kz, 2) - k2);
		std::complex<double> part_b = dkz * pow(x1, 2)*(x1*K1(x2*r0)*K0(x1*r0) - x2 * K0(x2*r0)*K1(x1*r0)) /
			(x1*K1(x2*r0)*I0(x1*r0) + x2 * K0(x2*r0)*I1(x1*r0));
		integral_L1 += cos(kz*(double)param.L1)*part_b;
		integral_L2 += cos(kz*(double)param.L2)*part_b;
		//iteration2++;
	}

	integral_L1 /= PI;
	integral_L2 /= PI;

	integral_L1 += inf_L1;
	integral_L2 += inf_L2;

	Vzz = static_cast<float>(arg(integral_L2 / integral_L1));
	//Vzz = arg(inf_L2 / inf_L1);
	return Vzz;
}
