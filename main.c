//如果是Windows的话，调用系统API ShellExecuteA打开图片
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#define USE_SHELL_OPEN
#endif

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
//ref:https://github.com/nothings/stb/blob/master/stb_image.h
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h" 
//ref:https://github.com/serge-rgb/TinyJPEG/blob/master/tiny_jpeg.h
#include <math.h>
#include <io.h>    
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

//计时 
#include <stdint.h>
#if   defined(__APPLE__)
# include <mach/mach_time.h>
#elif defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#else // __linux
# include <time.h>
# ifndef  CLOCK_MONOTONIC //_RAW
#  define CLOCK_MONOTONIC CLOCK_REALTIME
# endif
#endif
static
uint64_t nanotimer() {
	static int ever = 0;
#if defined(__APPLE__)
	static mach_timebase_info_data_t frequency;
	if (!ever) {
		if (mach_timebase_info(&frequency) != KERN_SUCCESS) {
			return 0;
		}
		ever = 1;
	}
	return;
#elif defined(_WIN32)
	static LARGE_INTEGER frequency;
	if (!ever) {
		QueryPerformanceFrequency(&frequency);
		ever = 1;
	}
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return (t.QuadPart * (uint64_t)1e9) / frequency.QuadPart;
#else // __linux
	struct timespec t;
	if (!ever) {
		if (clock_gettime(CLOCK_MONOTONIC, &spec) != 0) {
			return 0;
		}
		ever = 1;
	}
	clock_gettime(CLOCK_MONOTONIC, &spec);
	return (t.tv_sec * (uint64_t)1e9) + t.tv_nsec;
#endif
}


static double now()
{
	static uint64_t epoch = 0;
	if (!epoch) {
		epoch = nanotimer();
	}
	return (nanotimer() - epoch) / 1e9;
};

double  calcElapsed(double start, double end)
{
	double took = -start;
	return took + end;
}



//存储当前传入文件位置的变量
char  saveFile[1024];
//加载图片
unsigned char * loadImage(const char *filename, int *Width, int *Height, int *Channels)
{
	return   stbi_load(filename, Width, Height, Channels, 0);
}
//保存图片
void saveImage(const char *filename, int Width, int Height, int Channels, unsigned char *Output)
{

	memcpy(saveFile + strlen(saveFile), filename, strlen(filename));
	*(saveFile + strlen(saveFile) + 1) = 0;
	//保存为jpg
	if (!tje_encode_to_file(saveFile, Width, Height, Channels, true, Output))
	{
		fprintf(stderr, "写入 JPEG 文件失败.\n");
		return;
	}

#ifdef USE_SHELL_OPEN 
	ShellExecuteA(NULL, "open", saveFile, NULL, NULL, SW_SHOW);
#else
	//其他平台暂不实现
#endif
}


#ifndef ClampToByte
#define  ClampToByte(  v )  ( ((unsigned)(int)(v)) <(255) ? (v) : ((int)(v) < 0) ? (0) : (255)) 
#endif 

#define M_PI 3.14159265358979323846f

typedef struct cpu_HoughLine
{
	float Theta;
	int Radius;
	int Intensity;
	float RelativeIntensity;
} cpu_HoughLine;


typedef struct cpu_rect
{
	int  x;
	int  y;
	int  Width;
	int  Height;
} cpu_rect;

#ifndef clamp
#define clamp(value,min,max)  ((value) > (max )? (max ): (value) < (min) ? (min) : (value))
#endif 

#define FAST_MATH_TABLE_SIZE  512

const float sinTable_f32[FAST_MATH_TABLE_SIZE + 1] = {
	0.00000000f, 0.01227154f, 0.02454123f, 0.03680722f, 0.04906767f, 0.06132074f,
	0.07356456f, 0.08579731f, 0.09801714f, 0.11022221f, 0.12241068f, 0.13458071f,
	0.14673047f, 0.15885814f, 0.17096189f, 0.18303989f, 0.19509032f, 0.20711138f,
	0.21910124f, 0.23105811f, 0.24298018f, 0.25486566f, 0.26671276f, 0.27851969f,
	0.29028468f, 0.30200595f, 0.31368174f, 0.32531029f, 0.33688985f, 0.34841868f,
	0.35989504f, 0.37131719f, 0.38268343f, 0.39399204f, 0.40524131f, 0.41642956f,
	0.42755509f, 0.43861624f, 0.44961133f, 0.46053871f, 0.47139674f, 0.48218377f,
	0.49289819f, 0.50353838f, 0.51410274f, 0.52458968f, 0.53499762f, 0.54532499f,
	0.55557023f, 0.56573181f, 0.57580819f, 0.58579786f, 0.59569930f, 0.60551104f,
	0.61523159f, 0.62485949f, 0.63439328f, 0.64383154f, 0.65317284f, 0.66241578f,
	0.67155895f, 0.68060100f, 0.68954054f, 0.69837625f, 0.70710678f, 0.71573083f,
	0.72424708f, 0.73265427f, 0.74095113f, 0.74913639f, 0.75720885f, 0.76516727f,
	0.77301045f, 0.78073723f, 0.78834643f, 0.79583690f, 0.80320753f, 0.81045720f,
	0.81758481f, 0.82458930f, 0.83146961f, 0.83822471f, 0.84485357f, 0.85135519f,
	0.85772861f, 0.86397286f, 0.87008699f, 0.87607009f, 0.88192126f, 0.88763962f,
	0.89322430f, 0.89867447f, 0.90398929f, 0.90916798f, 0.91420976f, 0.91911385f,
	0.92387953f, 0.92850608f, 0.93299280f, 0.93733901f, 0.94154407f, 0.94560733f,
	0.94952818f, 0.95330604f, 0.95694034f, 0.96043052f, 0.96377607f, 0.96697647f,
	0.97003125f, 0.97293995f, 0.97570213f, 0.97831737f, 0.98078528f, 0.98310549f,
	0.98527764f, 0.98730142f, 0.98917651f, 0.99090264f, 0.99247953f, 0.99390697f,
	0.99518473f, 0.99631261f, 0.99729046f, 0.99811811f, 0.99879546f, 0.99932238f,
	0.99969882f, 0.99992470f, 1.00000000f, 0.99992470f, 0.99969882f, 0.99932238f,
	0.99879546f, 0.99811811f, 0.99729046f, 0.99631261f, 0.99518473f, 0.99390697f,
	0.99247953f, 0.99090264f, 0.98917651f, 0.98730142f, 0.98527764f, 0.98310549f,
	0.98078528f, 0.97831737f, 0.97570213f, 0.97293995f, 0.97003125f, 0.96697647f,
	0.96377607f, 0.96043052f, 0.95694034f, 0.95330604f, 0.94952818f, 0.94560733f,
	0.94154407f, 0.93733901f, 0.93299280f, 0.92850608f, 0.92387953f, 0.91911385f,
	0.91420976f, 0.90916798f, 0.90398929f, 0.89867447f, 0.89322430f, 0.88763962f,
	0.88192126f, 0.87607009f, 0.87008699f, 0.86397286f, 0.85772861f, 0.85135519f,
	0.84485357f, 0.83822471f, 0.83146961f, 0.82458930f, 0.81758481f, 0.81045720f,
	0.80320753f, 0.79583690f, 0.78834643f, 0.78073723f, 0.77301045f, 0.76516727f,
	0.75720885f, 0.74913639f, 0.74095113f, 0.73265427f, 0.72424708f, 0.71573083f,
	0.70710678f, 0.69837625f, 0.68954054f, 0.68060100f, 0.67155895f, 0.66241578f,
	0.65317284f, 0.64383154f, 0.63439328f, 0.62485949f, 0.61523159f, 0.60551104f,
	0.59569930f, 0.58579786f, 0.57580819f, 0.56573181f, 0.55557023f, 0.54532499f,
	0.53499762f, 0.52458968f, 0.51410274f, 0.50353838f, 0.49289819f, 0.48218377f,
	0.47139674f, 0.46053871f, 0.44961133f, 0.43861624f, 0.42755509f, 0.41642956f,
	0.40524131f, 0.39399204f, 0.38268343f, 0.37131719f, 0.35989504f, 0.34841868f,
	0.33688985f, 0.32531029f, 0.31368174f, 0.30200595f, 0.29028468f, 0.27851969f,
	0.26671276f, 0.25486566f, 0.24298018f, 0.23105811f, 0.21910124f, 0.20711138f,
	0.19509032f, 0.18303989f, 0.17096189f, 0.15885814f, 0.14673047f, 0.13458071f,
	0.12241068f, 0.11022221f, 0.09801714f, 0.08579731f, 0.07356456f, 0.06132074f,
	0.04906767f, 0.03680722f, 0.02454123f, 0.01227154f, 0.00000000f, -0.01227154f,
	-0.02454123f, -0.03680722f, -0.04906767f, -0.06132074f, -0.07356456f,
	-0.08579731f, -0.09801714f, -0.11022221f, -0.12241068f, -0.13458071f,
	-0.14673047f, -0.15885814f, -0.17096189f, -0.18303989f, -0.19509032f,
	-0.20711138f, -0.21910124f, -0.23105811f, -0.24298018f, -0.25486566f,
	-0.26671276f, -0.27851969f, -0.29028468f, -0.30200595f, -0.31368174f,
	-0.32531029f, -0.33688985f, -0.34841868f, -0.35989504f, -0.37131719f,
	-0.38268343f, -0.39399204f, -0.40524131f, -0.41642956f, -0.42755509f,
	-0.43861624f, -0.44961133f, -0.46053871f, -0.47139674f, -0.48218377f,
	-0.49289819f, -0.50353838f, -0.51410274f, -0.52458968f, -0.53499762f,
	-0.54532499f, -0.55557023f, -0.56573181f, -0.57580819f, -0.58579786f,
	-0.59569930f, -0.60551104f, -0.61523159f, -0.62485949f, -0.63439328f,
	-0.64383154f, -0.65317284f, -0.66241578f, -0.67155895f, -0.68060100f,
	-0.68954054f, -0.69837625f, -0.70710678f, -0.71573083f, -0.72424708f,
	-0.73265427f, -0.74095113f, -0.74913639f, -0.75720885f, -0.76516727f,
	-0.77301045f, -0.78073723f, -0.78834643f, -0.79583690f, -0.80320753f,
	-0.81045720f, -0.81758481f, -0.82458930f, -0.83146961f, -0.83822471f,
	-0.84485357f, -0.85135519f, -0.85772861f, -0.86397286f, -0.87008699f,
	-0.87607009f, -0.88192126f, -0.88763962f, -0.89322430f, -0.89867447f,
	-0.90398929f, -0.90916798f, -0.91420976f, -0.91911385f, -0.92387953f,
	-0.92850608f, -0.93299280f, -0.93733901f, -0.94154407f, -0.94560733f,
	-0.94952818f, -0.95330604f, -0.95694034f, -0.96043052f, -0.96377607f,
	-0.96697647f, -0.97003125f, -0.97293995f, -0.97570213f, -0.97831737f,
	-0.98078528f, -0.98310549f, -0.98527764f, -0.98730142f, -0.98917651f,
	-0.99090264f, -0.99247953f, -0.99390697f, -0.99518473f, -0.99631261f,
	-0.99729046f, -0.99811811f, -0.99879546f, -0.99932238f, -0.99969882f,
	-0.99992470f, -1.00000000f, -0.99992470f, -0.99969882f, -0.99932238f,
	-0.99879546f, -0.99811811f, -0.99729046f, -0.99631261f, -0.99518473f,
	-0.99390697f, -0.99247953f, -0.99090264f, -0.98917651f, -0.98730142f,
	-0.98527764f, -0.98310549f, -0.98078528f, -0.97831737f, -0.97570213f,
	-0.97293995f, -0.97003125f, -0.96697647f, -0.96377607f, -0.96043052f,
	-0.95694034f, -0.95330604f, -0.94952818f, -0.94560733f, -0.94154407f,
	-0.93733901f, -0.93299280f, -0.92850608f, -0.92387953f, -0.91911385f,
	-0.91420976f, -0.90916798f, -0.90398929f, -0.89867447f, -0.89322430f,
	-0.88763962f, -0.88192126f, -0.87607009f, -0.87008699f, -0.86397286f,
	-0.85772861f, -0.85135519f, -0.84485357f, -0.83822471f, -0.83146961f,
	-0.82458930f, -0.81758481f, -0.81045720f, -0.80320753f, -0.79583690f,
	-0.78834643f, -0.78073723f, -0.77301045f, -0.76516727f, -0.75720885f,
	-0.74913639f, -0.74095113f, -0.73265427f, -0.72424708f, -0.71573083f,
	-0.70710678f, -0.69837625f, -0.68954054f, -0.68060100f, -0.67155895f,
	-0.66241578f, -0.65317284f, -0.64383154f, -0.63439328f, -0.62485949f,
	-0.61523159f, -0.60551104f, -0.59569930f, -0.58579786f, -0.57580819f,
	-0.56573181f, -0.55557023f, -0.54532499f, -0.53499762f, -0.52458968f,
	-0.51410274f, -0.50353838f, -0.49289819f, -0.48218377f, -0.47139674f,
	-0.46053871f, -0.44961133f, -0.43861624f, -0.42755509f, -0.41642956f,
	-0.40524131f, -0.39399204f, -0.38268343f, -0.37131719f, -0.35989504f,
	-0.34841868f, -0.33688985f, -0.32531029f, -0.31368174f, -0.30200595f,
	-0.29028468f, -0.27851969f, -0.26671276f, -0.25486566f, -0.24298018f,
	-0.23105811f, -0.21910124f, -0.20711138f, -0.19509032f, -0.18303989f,
	-0.17096189f, -0.15885814f, -0.14673047f, -0.13458071f, -0.12241068f,
	-0.11022221f, -0.09801714f, -0.08579731f, -0.07356456f, -0.06132074f,
	-0.04906767f, -0.03680722f, -0.02454123f, -0.01227154f, -0.00000000f
};

inline float  fastSin(
	float x)
{
	float sinVal, fract, in;
	unsigned short  index;
	float a, b;
	int n;
	float findex;

	in = x * 0.159154943092f;

	n = (int)in;

	if (x < 0.0f)
	{
		n--;
	}

	in = in - (float)n;

	findex = (float)FAST_MATH_TABLE_SIZE * in;
	if (findex >= 512.0f) {
		findex -= 512.0f;
	}

	index = ((unsigned short)findex) & 0x1ff;

	fract = findex - (float)index;

	a = sinTable_f32[index];
	b = sinTable_f32[index + 1];

	sinVal = (1.0f - fract)*a + fract*b;

	return (sinVal);
}

inline float  fastCos(
	float x)
{
	float cosVal, fract, in;
	unsigned short index;
	float a, b;
	int n;
	float findex;

	in = x * 0.159154943092f + 0.25f;

	n = (int)in;

	if (in < 0.0f)
	{
		n--;
	}

	in = in - (float)n;

	findex = (float)FAST_MATH_TABLE_SIZE * in;
	index = ((unsigned short)findex) & 0x1ff;

	fract = findex - (float)index;

	a = sinTable_f32[index];
	b = sinTable_f32[index + 1];

	cosVal = (1.0f - fract)*a + fract*b;

	return (cosVal);
}

void CPUImageGrayscaleFilter(unsigned char* Input, unsigned char* Output, int  Width, int  Height, int Stride)
{
	int Channels = Stride / Width;

	const int B_WT = (int)(0.114 * 256 + 0.5);
	const int G_WT = (int)(0.587 * 256 + 0.5);
	const int R_WT = 256 - B_WT - G_WT;            //     int(0.299 * 256 + 0.5);
	int Channel = Stride / Width;
	if (Channel == 3)
	{
		for (int Y = 0; Y < Height; Y++)
		{
			unsigned char *LinePS = Input + Y * Stride;
			unsigned char *LinePD = Output + Y * Width;
			int X = 0;
			for (; X < Width - 4; X += 4, LinePS += Channel * 4)
			{
				LinePD[X + 0] = (B_WT * LinePS[0] + G_WT * LinePS[1] + R_WT * LinePS[2]) >> 8;
				LinePD[X + 1] = (B_WT * LinePS[3] + G_WT * LinePS[4] + R_WT * LinePS[5]) >> 8;
				LinePD[X + 2] = (B_WT * LinePS[6] + G_WT * LinePS[7] + R_WT * LinePS[8]) >> 8;
				LinePD[X + 3] = (B_WT * LinePS[9] + G_WT * LinePS[10] + R_WT * LinePS[11]) >> 8;
			}
			for (; X < Width; X++, LinePS += Channel)
			{
				LinePD[X] = (B_WT * LinePS[0] + G_WT * LinePS[1] + R_WT * LinePS[2]) >> 8;
			}
		}
	}
	else if (Channel == 4)
	{
		for (int Y = 0; Y < Height; Y++)
		{
			unsigned char *LinePS = Input + Y * Stride;
			unsigned char *LinePD = Output + Y * Width;
			int X = 0;
			for (; X < Width - 4; X += 4, LinePS += Channel * 4)
			{
				LinePD[X + 0] = (B_WT * LinePS[0] + G_WT * LinePS[1] + R_WT * LinePS[2]) >> 8;
				LinePD[X + 1] = (B_WT * LinePS[4] + G_WT * LinePS[5] + R_WT * LinePS[6]) >> 8;
				LinePD[X + 2] = (B_WT * LinePS[8] + G_WT * LinePS[9] + R_WT * LinePS[10]) >> 8;
				LinePD[X + 3] = (B_WT * LinePS[12] + G_WT * LinePS[13] + R_WT * LinePS[14]) >> 8;
			}
			for (; X < Width; X++, LinePS += Channel)
			{
				LinePD[X] = (B_WT * LinePS[0] + G_WT * LinePS[1] + R_WT * LinePS[2]) >> 8;
			}
		}
	}
	else if (Channel == 1)
	{
		if (Output != Input)
		{
			memcpy(Output, Input, Height*Stride);
		}
	}
}

void CPUImageColorInvertFilter(unsigned char* Input, unsigned char* Output, int  Width, int  Height, int Stride)
{
	int Channels = Stride / Width; unsigned char invertMap[256] = { 0 };
	for (int pixel = 0; pixel < 256; pixel++)
	{
		invertMap[pixel] = (255 - pixel);
	}
	if (Channels == 1) {
		for (int Y = 0; Y < Height; Y++)
		{
			unsigned char*     pOutput = Output + (Y * Stride);
			unsigned char*     pInput = Input + (Y * Stride);
			for (int X = 0; X < Width; X++)
			{
				pOutput[X] = invertMap[pInput[X]];
			}
		}
	}
	else
	{

		for (int Y = 0; Y < Height; Y++)
		{
			unsigned char*     pOutput = Output + (Y * Stride);
			unsigned char*     pInput = Input + (Y * Stride);
			for (int X = 0; X < Width; X++)
			{
				pOutput[0] = invertMap[pInput[0]];
				pOutput[1] = invertMap[pInput[1]];
				pOutput[2] = invertMap[pInput[2]];
				pInput += Channels;
				pOutput += Channels;
			}
		}
	}
}
float  CPUImageCalcSkewAngle(unsigned char* Input, int Width, int Height, cpu_rect *CheckRectPtr, int maxSkewToDetect, int stepsPerDegree, int localPeakRadius, int nLineCount)
{
	cpu_rect CheckRect = *CheckRectPtr;
	//确定指定的区域在原图片范围内
	CheckRect.x = clamp(CheckRect.x, 0, Width - 1);
	CheckRect.y = clamp(CheckRect.y, 0, Height - 1);
	CheckRect.Width = clamp(CheckRect.Width, 1, Width - 1);
	CheckRect.Height = clamp(CheckRect.Height, 1, Height - 1);

	// 处理参数
	maxSkewToDetect = clamp(maxSkewToDetect, 0, 91);
	localPeakRadius = clamp(localPeakRadius, 1, 10);
	stepsPerDegree = clamp(stepsPerDegree, 1, 10);
	int    houghHeight = (2 * maxSkewToDetect * stepsPerDegree);
	float    thetaStep = (2 * maxSkewToDetect * M_PI / 180) / houghHeight;
	int halfWidth = Width >> 1;
	int halfHeight = Height >> 1;
	// 计算 Hough 映射宽度
	int halfHoughWidth = (int)sqrtf((float)(halfWidth * halfWidth + halfHeight * halfHeight));
	int houghWidth = (halfHoughWidth * 2);
	float minTheta = 90.0f - maxSkewToDetect;
	unsigned short * houghMap = (unsigned short *)calloc(houghHeight*houghWidth, sizeof(unsigned short));
	float* sinMap = (float*)malloc(houghHeight * sizeof(float));
	float* cosMap = (float*)malloc(houghHeight * sizeof(float));
	cpu_HoughLine* HoughLines = (cpu_HoughLine*)calloc(houghHeight*houghWidth, sizeof(cpu_HoughLine));
	if (houghMap == NULL || sinMap == NULL || cosMap == NULL || HoughLines == NULL)
	{
		if (houghMap)
		{
			free(houghMap);
			houghMap = NULL;
		}
		if (sinMap)
		{
			free(sinMap);
			sinMap = NULL;
		}
		if (cosMap)
		{
			free(cosMap);
			cosMap = NULL;
		}
		if (HoughLines)
		{
			free(HoughLines);
			HoughLines = NULL;
		}
		return 0.0f;
	}
	else
	{
		// 预计算 Sin 与 Cos表
		float mt = (minTheta * M_PI / 180.0f);
		for (int i = 0; i < houghHeight; i++)
		{
			float cur_weight = mt + (i * thetaStep);
			sinMap[i] = fastSin(cur_weight);
			cosMap[i] = fastCos(cur_weight);
		}
	}
	int startX = -halfWidth + CheckRect.x;
	int startY = -halfHeight + CheckRect.y;
	int stopX = Width - halfWidth - (Width - CheckRect.Width);
	int stopY = Height - halfHeight - (Height - CheckRect.Height) - 1;
	int offset = Width - CheckRect.Width;


	unsigned char* src = Input + CheckRect.y *  Width + CheckRect.x;
	unsigned char* srcBelow = src + Width;

	for (int Y = startY; Y < stopY; Y++)
	{
		for (int X = startX; X < stopX; X++, src++, srcBelow++)
		{
			if ((*src < 128) && (*srcBelow >= 128))
			{
				for (int theta = 0; theta < houghHeight; theta++)
				{
					int radius = (int)(cosMap[theta] * X - sinMap[theta] * Y) + halfHoughWidth;

					if ((radius < 0) || (radius >= houghWidth))
					{
						continue;
					}

					houghMap[theta*houghWidth + radius]++;
				}
			}
		}
		src += offset;
		srcBelow += offset;
	}


	// 找到 Hough映射的最大值
	float maxMapIntensity = 0.0000000001f;
	for (int theta = 0; theta < houghHeight; theta++)
	{
		unsigned short * houghMapLine = houghMap + theta*houghWidth;
		for (int radius = 0; radius < houghWidth; radius++)
		{
			maxMapIntensity = max(maxMapIntensity, houghMapLine[radius]);
		}
	}
	int minLineIntensity = Width / 10;

	// 收集大于或等于指定强度的直线

	int lineIntensity = 0;
	bool foundGreater = false;
	int lineSize = 0;
	for (int theta = 0; theta < houghHeight; theta++)
	{
		unsigned short * houghMapLine = houghMap + theta*houghWidth;
		for (int radius = 0; radius < houghWidth; radius++)
		{
			// 取当前强度
			lineIntensity = houghMapLine[radius];

			if (lineIntensity < minLineIntensity)
			{
				continue;
			}

			foundGreater = false;

			// 检查邻边
			for (int t = theta - localPeakRadius, ttMax = theta + localPeakRadius; t < ttMax; t++)
			{
				//跳过map值
				if (t < 0)
				{
					continue;
				}
				if (t >= houghHeight)
				{
					break;
				}

				//如果不是局部最大则跳出
				if (foundGreater == true)
				{
					break;
				}
				for (int r = radius - localPeakRadius, trMax = radius + localPeakRadius; r < trMax; r++)
				{
					//跳过map值
					if (r < 0)
					{
						continue;
					}
					if (r >= houghWidth)
					{
						break;
					}
					// 当前值与邻边对比
					if (houghMap[t*houghWidth + r] > lineIntensity)
					{
						foundGreater = true;
						break;
					}
				}
			}
			// 可能是局部最大值,记录下来
			if (!foundGreater)
			{
				cpu_HoughLine tempVar;
				tempVar.Theta = 90.0f - maxSkewToDetect + (theta) / stepsPerDegree;
				tempVar.Radius = (radius - halfHoughWidth);
				tempVar.Intensity = lineIntensity;
				tempVar.RelativeIntensity = lineIntensity / maxMapIntensity;
				HoughLines[lineSize] = tempVar;
				lineSize++;
			}
		}
	}

	float skewAngle = 0;
	if (lineSize > 0)
	{
		//排序，从大到小 
		cpu_HoughLine temp;
		for (int i = 0; i < lineSize; i++)
		{
			for (int j = 0; j < lineSize - 1; j++)
			{
				if (HoughLines[j].Intensity < HoughLines[j + 1].Intensity)
				{
					temp = HoughLines[j + 1];
					HoughLines[j + 1] = HoughLines[j];
					HoughLines[j] = temp;
				}
			}
		}

		int n = min(nLineCount, lineSize);

		float sumIntensity = 0;

		for (int i = 0; i < n; i++)
		{
			if (HoughLines[i].RelativeIntensity > 0.5f)
			{
				skewAngle += (HoughLines[i].Theta * HoughLines[i].RelativeIntensity);
				sumIntensity += HoughLines[i].RelativeIntensity;
			}
		}
		skewAngle = skewAngle / sumIntensity;
	}
	if (houghMap)
	{
		free(houghMap);
		houghMap = NULL;
	}
	if (sinMap)
	{
		free(sinMap);
		sinMap = NULL;
	}
	if (cosMap)
	{
		free(cosMap);
		cosMap = NULL;
	}
	if (HoughLines)
	{
		free(HoughLines);
		HoughLines = NULL;
	}
	if (skewAngle != 0)
	{
		return skewAngle - 90.0f;
	}
	return skewAngle;
}

void CPUImageRotateBilinear(unsigned char * Input, int Width, int Height, int Stride, unsigned char * Output, int outWidth, int outHeight, float angle, bool keepSize, int fillColorR, int fillColorG, int fillColorB)
{
	if (Input == NULL || Output == NULL) return;

	float  oldXradius = (float)(Width - 1) / 2;
	float  oldYradius = (float)(Height - 1) / 2;

	// 输出图像的半径大小

	float  newXradius = (float)(outWidth - 1) / 2;
	float  newYradius = (float)(outHeight - 1) / 2;

	// 角度的正弦和余弦
	float angleRad = -angle * M_PI / 180.0f;
	float angleCos = fastCos(angleRad);
	float angleSin = fastSin(angleRad);
	int Channels = Stride / Width;
	int dstOffset = outWidth*Channels - ((Channels == 1) ? outWidth : outWidth * Channels);

	// 背景色
	unsigned char fillR = fillColorR;
	unsigned char fillG = fillColorG;
	unsigned char fillB = fillColorB;
	// 临界点
	int lastHeight = Height - 1;
	int lastWidth = Width - 1;
	// 四点指针   
	unsigned char* src = (unsigned char*)Input;
	unsigned char* dst = (unsigned char*)Output;
	// cx, cy  目标像素的相对于图像中心的坐标 
	if (Channels == 1)
	{
		float cy = -newYradius;
		for (int y = 0; y < outHeight; y++)
		{
			const 	float	tx = angleSin * cy + oldXradius;
			const float	ty = angleCos * cy + oldYradius;

			float cx = -newXradius;
			for (int x = 0; x < outWidth; x++, dst++)
			{
				// 初始起点位置
				const 	float	ox = tx + angleCos * cx;
				const 	float	oy = ty - angleSin * cx;

				const int	ox1 = (int)ox;
				const int	oy1 = (int)oy;

				// 判断是否为有效区域 
				if ((ox1 < 0) || (oy1 < 0) || (ox1 >= Width) || (oy1 >= Height))
				{
					// 无效区域填充背景 
					*dst = fillG;
				}
				else
				{
					// 边界点处理 
					const int	ox2 = (ox1 == lastWidth) ? ox1 : ox1 + 1;
					const int	oy2 = (oy1 == lastHeight) ? oy1 : oy1 + 1;
					float dx1 = ox - (float)ox1;
					if (dx1 < 0)
						dx1 = 0;
					const 	float dx2 = 1.0f - dx1;
					float dy1 = oy - (float)oy1;
					if (dy1 < 0)
						dy1 = 0;
					const 	float dy2 = 1.0f - dy1;

					unsigned char*p1 = src + oy1 * Stride;
					unsigned char*	p2 = src + oy2 * Stride;
					// 进行四点插值
					*dst = (unsigned char)(
						dy2 * (dx2 * p1[ox1] + dx1 * p1[ox2]) +
						dy1 * (dx2 * p2[ox1] + dx1 * p2[ox2]));
				}
				cx++;
			}
			cy++;
			dst += dstOffset;
		}
	}
	else
	{
		float cy = -newYradius;
		for (int y = 0; y < outHeight; y++)
		{
			const 	float 	tx = angleSin * cy + oldXradius;
			const 	float 	ty = angleCos * cy + oldYradius;

			float cx = -newXradius;
			for (int x = 0; x < outWidth; x++, dst += Channels)
			{
				// 初始起点位置
				const 	float ox = tx + angleCos * cx;
				const 	float oy = ty - angleSin * cx;
				const int	ox1 = (int)ox;
				const int	oy1 = (int)oy;

				// 判断是否为有效区域 
				if ((ox1 < 0) || (oy1 < 0) || (ox1 >= Width) || (oy1 >= Height))
				{
					// 无效区域填充背景 
					dst[0] = fillR;
					dst[1] = fillG;
					dst[2] = fillB;
				}
				else
				{
					// 边界点处理 
					const int	ox2 = (ox1 == lastWidth) ? ox1 : ox1 + 1;
					const int	oy2 = (oy1 == lastHeight) ? oy1 : oy1 + 1;
					float dx1 = ox - (float)ox1;
					if (dx1 < 0)
						dx1 = 0;
					const	float dx2 = 1.0f - dx1;
					float dy1 = oy - (float)oy1;
					if (dy1 < 0)
						dy1 = 0;
					const	float	dy2 = 1.0f - dy1;

					// 计算四点的坐标
					unsigned char*	p1 = src + oy1 * Stride;
					unsigned char*  p2 = p1;
					p1 += ox1 * Channels;
					p2 += ox2 * Channels;

					unsigned char* p3 = src + oy2 * Stride;
					unsigned char* p4 = p3;
					p3 += ox1 * Channels;
					p4 += ox2 * Channels;

					// 进行四点插值
					dst[0] = (unsigned char)(
						dy2 * (dx2 * p1[0] + dx1 * p2[0]) +
						dy1 * (dx2 * p3[0] + dx1 * p4[0]));
					dst[1] = (unsigned char)(
						dy2 * (dx2 * p1[1] + dx1 * p2[1]) +
						dy1 * (dx2 * p3[1] + dx1 * p4[1]));
					dst[2] = (unsigned char)(
						dy2 * (dx2 * p1[2] + dx1 * p2[2]) +
						dy1 * (dx2 * p3[2] + dx1 * p4[2]));
				}
				cx++;
			}
			cy++;
			dst += dstOffset;
		}
	}
}

bool CPUImageIsTextImage(unsigned char * Input, int Width, int Height)
{
	const int blacklimit = 20;
	const int greylimit = 140;
	const int contrast_offset = 80;

	int prev_color[256];
	int cur_color[256];

	for (int i = 0; i < 256; i++)
	{
		cur_color[i] = 0;
		prev_color[i] = 0;
	}

	for (int i = 0; i <= blacklimit; i++)
	{
		//黑色
		cur_color[i] = 100;
		prev_color[i] = 100000;
	}

	for (int i = blacklimit + 1 + contrast_offset; i <= greylimit; i++)
	{
		//灰色
		cur_color[i] = 10;
		prev_color[i] = 10000;
	}

	for (int i = greylimit + 1 + contrast_offset; i <= 255; i++)
	{
		//白色
		cur_color[i] = 1;
		prev_color[i] = 1000;
	}
	int line_count = 0;


	int n = -1;
	for (int y = 0; y < Height; y += 10)
	{
		n++;
		int	white_amt = 0;
		unsigned char *  buffer = Input + y*Width;
		int x = 0;
		for (x = 1; x < Width; x++)
		{
			const unsigned char 	prev_pixel = buffer[(x - 1)];
			const unsigned char 	cur_pixel = buffer[x];

			if ((prev_color[prev_pixel]) && (cur_color[cur_pixel]))
			{
				//是否是白色
				if ((prev_color[prev_pixel] + cur_color[cur_pixel]) == 1001)
				{
					white_amt++;
				}
			}
		}
		//白色的一行
		if (((float)white_amt / (float)x) > 0.85f)
		{
			line_count++;
		}
	}

	float line_count_ratio = (n != 0.f) ? (float)line_count / (float)n : 0.0f;

	if (line_count_ratio < 0.4f || line_count_ratio > 1.0f)
	{
		return false;
	}

	return true;
}

bool 	CPUImageDocumentDeskew(unsigned char * Input, unsigned char *Output, int Width, int Height, int Stride)
{
	if (Input == NULL || Output == NULL || Input == Output)
		return false;
	int Channels = Stride / Width;
	//最大倾斜角度 
	int maxSkewToDetect = 89;

	cpu_rect rect = { 0 };
	rect.Width = Width;
	rect.Height = Height;
	// 以最大权重的2条直线为基准计算倾斜角度
	int nLineCount = 2;
	//角度步进数
	int stepsPerDegree = 1;
	//局部临界半径
	int localPeakRadius = 10;
	CPUImageGrayscaleFilter(Input, Output, Width, Height, Stride);
	if (!CPUImageIsTextImage(Output, Width, Height))
	{
		CPUImageColorInvertFilter(Output, Output, Width, Height, Width);
	}
	float skewAngle = CPUImageCalcSkewAngle(Output, Width, Height, &rect, maxSkewToDetect, stepsPerDegree, localPeakRadius, nLineCount);
	if ((skewAngle == 0) || (skewAngle < -maxSkewToDetect || skewAngle >   maxSkewToDetect))
	{
		memcpy(Output, Input, Height* Stride * sizeof(unsigned char));
		return false;
	}
	else
	{
		CPUImageRotateBilinear(Input, Width, Height, Stride, Output, Width, Height, -skewAngle, true, 255, 255, 255);
	}
	return true;
}


//分割路径函数
void splitpath(const char* path, char* drv, char* dir, char* name, char* ext)
{
	const char* end;
	const char* p;
	const char* s;
	if (path[0] && path[1] == ':') {
		if (drv) {
			*drv++ = *path++;
			*drv++ = *path++;
			*drv = '\0';
		}
	}
	else if (drv)
		*drv = '\0';
	for (end = path; *end && *end != ':';)
		end++;
	for (p = end; p > path && *--p != '\\' && *p != '/';)
		if (*p == '.') {
			end = p;
			break;
		}
	if (ext)
		for (s = end; (*ext = *s++);)
			ext++;
	for (p = end; p > path;)
		if (*--p == '\\' || *p == '/') {
			p++;
			break;
		}
	if (name) {
		for (s = p; s < end;)
			*name++ = *s++;
		*name = '\0';
	}
	if (dir) {
		for (s = path; s < p;)
			*dir++ = *s++;
		*dir = '\0';
	}
}

//取当前传入的文件位置
void getCurrentFilePath(const char *filePath, char *saveFile)
{
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	splitpath(filePath, drive, dir, fname, ext);
	int n = strlen(filePath);
	memcpy(saveFile, filePath, n);
	char * cur_saveFile = saveFile + (n - strlen(ext));
	cur_saveFile[0] = '_';
	cur_saveFile[1] = 0;
}

int main(int argc, char **argv)
{
	printf("Image Processing \n ");
	printf("博客:http://tntmonks.cnblogs.com/ \n ");
	printf("支持解析如下图片格式: \n ");
	printf("JPG, PNG, TGA, BMP, PSD, GIF, HDR, PIC \n ");

	//检查参数是否正确

	if (argc < 2)
	{
		printf("参数错误。 \n ");
		printf("请拖放文件到可执行文件上，或使用命令行：imageProc.exe 图片 \n ");
		printf("请拖放文件例如: imageProc.exe d:\\image.jpg \n ");

		return 0;
	}

	char*szfile = argv[1];
	//检查输入的文件是否存在
	if (_access(szfile, 0) == -1)
	{
		printf("输入的文件不存在，参数错误！ \n ");
	}

	getCurrentFilePath(szfile, saveFile);

	int Width = 0;                    //图片宽度
	int Height = 0;                   //图片高度
	int Channels = 0;                 //图片通道数
	unsigned char *inputImage = NULL; //输入图片指针
	double startTime = now();
	//加载图片
	inputImage = loadImage(szfile, &Width, &Height, &Channels);

	double nLoadTime = calcElapsed(startTime, now());
	printf("加载耗时: %d 毫秒!\n ", (int)(nLoadTime * 1000));
	if ((Channels != 0) && (Width != 0) && (Height != 0))
	{
		//分配与载入同等内存用于处理后输出结果
		unsigned char *outputImg = (unsigned char *)stbi__malloc(Width * Channels * Height * sizeof(unsigned char));
		if (inputImage)
		{
			//如果图片加载成功，则将内容复制给输出内存，方便处理
			memcpy(outputImg, inputImage, Width * Channels * Height);
		}
		else
		{
			printf("加载文件: %s 失败!\n ", szfile);
		}
		startTime = now();
		//处理算法
		CPUImageDocumentDeskew(inputImage, outputImg, Width, Height, Width*Channels);
		double nProcessTime = calcElapsed(startTime, now());
		printf("处理耗时: %d 毫秒!\n ", (int)(nProcessTime * 1000));
		//保存处理后的图片
		startTime = now();

		saveImage("_done.jpg", Width, Height, Channels, outputImg);
		double nSaveTime = calcElapsed(startTime, now());

		printf("保存耗时: %d 毫秒!\n ", (int)(nSaveTime * 1000));
		//释放占用的内存
		if (outputImg)
		{
			stbi_image_free(outputImg);
			outputImg = NULL;
		}

		if (inputImage)
		{
			stbi_image_free(inputImage);
			inputImage = NULL;
		}
	}
	else
	{
		printf("加载文件: %s 失败!\n", szfile);
	}

	getchar();
	printf("按任意键退出程序 \n");

	return EXIT_SUCCESS;
}