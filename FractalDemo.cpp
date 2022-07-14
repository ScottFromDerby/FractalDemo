//	Scott Breen Fractal Demo 2022

#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include <thread>
#include <vector>

#include "SDL.h"
#include "SDL_ttf.h"
#include "floatx.hpp"

using namespace flx;

constexpr int INITIAL_WINDOW_WIDTH = 1000;
constexpr int INITIAL_WINDOW_HEIGHT = 800;
constexpr unsigned long NUM_THREADS = 32;
constexpr int RAW_BUFFER_SIZE = 2048 * 2048;

//	uncomment to try out floatx. *Very* slow
//typedef floatx<11, 52> bigdouble;
typedef double bigdouble;

SDL_Window* g_pMainWindow = nullptr;
SDL_Renderer* g_pMainRenderer = nullptr;
SDL_Texture* g_pMainTexture = nullptr;
SDL_Texture* g_pFontTexture = nullptr;
SDL_Texture* g_pSmallFontTexture = nullptr;
TTF_Font* g_pMainFont = nullptr;
TTF_Font* g_pSmallFont = nullptr;

int* g_pWorkingData = nullptr;
Uint32* g_pPixelBackbuffer = nullptr;

int g_iWindowWidth = INITIAL_WINDOW_WIDTH;
int g_iWindowHeight = INITIAL_WINDOW_HEIGHT;
bigdouble g_fFracLeft = -2.5;
bigdouble g_fFracTop = -2.0;
bigdouble g_fFracRight = 2.5;
bigdouble g_fFracBottom = 2.0;
int g_iNumFracIterations = 141;
int g_iNumIterationBase = 50;
int g_iLastFractalDurationMs = 0;
int g_iActiveMouseDragX = 0;
int g_iActiveMouseDragY = 0;
bool g_bActiveMouseDragging = false;
SDL_Rect g_smallFontRect;

template<typename T> constexpr T clamp(T val, T lower, T upper)
{
	return (val > upper) ? upper : ((val < lower) ? lower : val);
}
void HSVtoRGB(float H, float S, float V, int& R, int& G, int& B) 
{
	H = fmodf(H, 360.0f);
	//S = clamp(S, 0.0f, 100.0f);
	//V = clamp(V, 0.0f, 100.0f);

	if (H > 360.0f || H < 0.0f || S>100.0f || S < 0.0f || V>100.0f || V < 0.0f)
	{
		assert(!"bad range");
		return;
	}
	float s = S / 100.0f;
	float v = V / 100.0f;
	float C = s * v;
	float X = C * (1.0f - fabsf(fmodf(H / 60.0f, 2.0f) - 1.0f));
	float m = v - C;
	float r, g, b;
	if (H >= 0.0f && H < 60.0f) {
		r = C, g = X, b = 0;
	}
	else if (H >= 60.0f && H < 120.0f) {
		r = X, g = C, b = 0;
	}
	else if (H >= 120.0f && H < 180.0f) {
		r = 0, g = C, b = X;
	}
	else if (H >= 180.0f && H < 240.0f) {
		r = 0, g = X, b = C;
	}
	else if (H >= 240.0f && H < 300.0f) {
		r = X, g = 0, b = C;
	}
	else {
		r = C, g = 0, b = X;
	}
	R = static_cast<int>((r + m) * 255);
	G = static_cast<int>((g + m) * 255);
	B = static_cast<int>((b + m) * 255);
	//cout << "R : " << R << endl;
	//cout << "G : " << G << endl;
	//cout << "B : " << B << endl;
}
unsigned long ThreadWork_dirtySegmentsBitmask = 0;
bool ThreadWork_IsDirty(int Segment)
{
	return (ThreadWork_dirtySegmentsBitmask & (1 << Segment)) != 0;
}
void ThreadWork_Clean(int Segment)
{
	ThreadWork_dirtySegmentsBitmask &= ~(1 << Segment);
}
void ThreadWork_DirtyAll()
{
	ThreadWork_dirtySegmentsBitmask = (1ull << (NUM_THREADS)) - 1;
}

void putpixel(int x, int y, int r, int g, int b)
{
	const int a = 0xff;
	r %= 0xff;
	g %= 0xff;
	b %= 0xff;
	g_pPixelBackbuffer[x + (y * g_iWindowWidth)] = (r << 0) | (g << 8) | (b << 16) | (a << 24);
}

//	Function to draw mandelbrot set
void fractal(bigdouble left, bigdouble top, bigdouble right, bigdouble bottom, int iterationsToDo, int startY, int endY)
{
	// getting maximum value of x-axis of screen
	int maxx = g_iWindowWidth;

	// getting maximum value of y-axis of screen
	int maxy = g_iWindowHeight;

	bigdouble xside = right - left;
	bigdouble yside = bottom - top;

	// setting up the xscale and yscale
	bigdouble xscale = xside / maxx;
	bigdouble yscale = yside / maxy;

	// scanning every point in that rectangular area.
	// Each point represents a Complex number (x + yi).
	// Iterate that complex number
	for (int y = startY; y <= endY; y++)
	{
		for (int x = 1; x <= maxx - 1; x++)
		{
			// c_real
			bigdouble cx = x * xscale + left;

			// c_imaginary
			bigdouble cy = y * yscale + top;

			// z_real
			bigdouble zx = 0;

			// z_imaginary
			bigdouble zy = 0;

			int count = 0;

			// Calculate whether c(c_real + c_imaginary) belongs
			// to the Mandelbrot set or not and draw a pixel
			// at coordinates (x, y) accordingly
			// If you reach the Maximum number of iterations
			// and If the distance from the origin is
			// greater than 2 exit the loop
			while ((zx * zx + zy * zy < 4) && (count < iterationsToDo))
			{
				// Calculate Mandelbrot function
				// z = z*z + c where z is a complex number

				// tempx = z_real*_real - z_imaginary*z_imaginary + c_real
				bigdouble tempx = zx * zx - zy * zy + cx;

				// 2*z_real*z_imaginary + c_imaginary
				zy = 2 * zx * zy + cy;

				// Updating z_real = tempx
				zx = tempx;

				// Increment count
				count = count + 1;
			}

			g_pWorkingData[x + (y * g_iWindowWidth)] = count;

			int r = 0, g = 0, b = 0;
			HSVtoRGB(360.0f * count / iterationsToDo, 100.0f, 100.0f, r, g, b);
			putpixel(x, y, r, g, b);
		}
	}
}

void fractal_gen_thread(int Proportion)
{
	while (true)
	{
		if (ThreadWork_IsDirty(Proportion))
		{
			const int NumLinesPerSegment = (g_iWindowHeight / NUM_THREADS) + 1;
			fractal(g_fFracLeft, g_fFracTop, g_fFracRight, g_fFracBottom, g_iNumFracIterations, NumLinesPerSegment * Proportion, NumLinesPerSegment * (Proportion + 1));
			ThreadWork_Clean(Proportion);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void create_help_text()
{
	SDL_Surface* pSurface = TTF_RenderText_Blended(g_pSmallFont, "r = reset   lmb & drag to move   mousewheel = zoom   +/- change granularity", { 255, 255, 255, 0 });
	if(g_pSmallFontTexture != nullptr)
	{
		SDL_DestroyTexture(g_pSmallFontTexture);
	}
	g_pSmallFontTexture = SDL_CreateTextureFromSurface(g_pMainRenderer, pSurface);
	g_smallFontRect = { 4, g_iWindowHeight - pSurface->h - 4, pSurface->w, pSurface->h };
	SDL_FreeSurface(pSurface);
}

int SDL_main(int argc, char* argv[])
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		return -1;
	}

	g_pMainWindow = SDL_CreateWindow("FractalDemo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_iWindowWidth, g_iWindowHeight, SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	SDL_assert(g_pMainWindow != nullptr);

	g_pMainRenderer = SDL_CreateRenderer(g_pMainWindow, -1, 0);
	SDL_assert(g_pMainRenderer != nullptr);

	g_pMainTexture = SDL_CreateTexture(g_pMainRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, g_iWindowWidth, g_iWindowHeight);
	SDL_assert(g_pMainTexture != nullptr);

	TTF_Init();
	g_pMainFont = TTF_OpenFont("tahoma.ttf", 18);
	if (g_pMainFont == nullptr)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Missing font", "Tahoma.ttf is missing!", nullptr);
	}
	g_pSmallFont = TTF_OpenFont("tahoma.ttf", 16);

	g_pWorkingData = new int[RAW_BUFFER_SIZE];
	memset(g_pWorkingData, 0, RAW_BUFFER_SIZE);
	g_pPixelBackbuffer = new Uint32[RAW_BUFFER_SIZE * sizeof(Uint32)]; //  just use as raw buffer

	SDL_Event lastEvent;
	bool bDirty = true;

	char lastFontBuffer[256];
	lastFontBuffer[0] = '\0';
	SDL_Rect fontRect;

	create_help_text();

	std::vector<std::thread*> threads;
	for (int i = 0; i < NUM_THREADS; ++i)
	{
		threads.push_back(new std::thread(fractal_gen_thread, i));
	}

	while (true)
	{
		if (bDirty)
		{
			//  Old: Singlethread
			//fractal(left, top, right, bottom, numIterations, 1, WINDOW_HEIGHT-1);

			//	Tell all threads they are dirty. Get them to recalculate the current image
			auto startTime = std::chrono::system_clock::now();
			ThreadWork_DirtyAll();
			while (ThreadWork_dirtySegmentsBitmask != 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				auto elapsed = std::chrono::system_clock::now() - startTime;
				g_iLastFractalDurationMs = static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
				printf("\rThread Status: %08x - Iterations: %04u - Time: %ums   ", ThreadWork_dirtySegmentsBitmask, g_iNumFracIterations, g_iLastFractalDurationMs);
			}
			printf("\n");

			SDL_UpdateTexture(g_pMainTexture, nullptr, g_pPixelBackbuffer, g_iWindowWidth * sizeof(Uint32));
			bDirty = false;
		}

		//	Main render loop
		SDL_RenderClear(g_pMainRenderer);

		//	Render fractal
		SDL_Rect rcDest = { g_iActiveMouseDragX, g_iActiveMouseDragY, g_iWindowWidth, g_iWindowHeight };
		SDL_RenderCopy(g_pMainRenderer, g_pMainTexture, nullptr, &rcDest);

		//	Update UI
		char buffer[1024];
		sprintf_s(buffer, "Threads: %d     At: (%1.6f, %1.6f, %1.6f, %1.6f)     Iterations: %d (%d%%)     Last Frac: %dms (%d/s)", 
			NUM_THREADS, (double)g_fFracLeft, (double)g_fFracTop, (double)g_fFracRight, (double)g_fFracBottom, g_iNumFracIterations, g_iNumIterationBase * 2, g_iLastFractalDurationMs, (int)(1000.0f/g_iLastFractalDurationMs));
		if (_stricmp(buffer, lastFontBuffer) != 0)
		{
			//	Only recalculate the font texture if necessary; it's expensive!
			SDL_Surface* pSurface = TTF_RenderText_Blended(g_pMainFont, buffer, { 255, 255, 255, 0 });
			SDL_DestroyTexture(g_pFontTexture);
			g_pFontTexture = SDL_CreateTextureFromSurface(g_pMainRenderer, pSurface);
			fontRect = { 4, 4, pSurface->w, pSurface->h };
			SDL_FreeSurface(pSurface);

			strcpy_s(lastFontBuffer, buffer);
		}

		//	Render font textures on top
		SDL_RenderCopy(g_pMainRenderer, g_pFontTexture, nullptr, &fontRect);
		SDL_RenderCopy(g_pMainRenderer, g_pSmallFontTexture, nullptr, &g_smallFontRect);

		SDL_RenderPresent(g_pMainRenderer);

		//	Update loop
		SDL_PumpEvents();
		if (SDL_PollEvent(&lastEvent))
		{
			if (lastEvent.type == SDL_MOUSEWHEEL)
			{
				bigdouble zoomStrength = g_iWindowWidth * 0.00004f;
				zoomStrength *= powf((float)abs(lastEvent.wheel.y), 1.4f);	//	exp. scrolling!
				if (lastEvent.wheel.y < 0.0f)
				{	
					//	Restore signage
					zoomStrength *= -1.0;
				}
				zoomStrength = clamp<bigdouble>(zoomStrength, -0.5, 0.5);

				//	Apply zoom to current mouse location
				int mouseX = 0;
				int mouseY = 0;
				SDL_GetMouseState(&mouseX, &mouseY);

				const bigdouble xProportion = mouseX / (bigdouble)g_iWindowWidth;
				const bigdouble yProportion = mouseY / (bigdouble)g_iWindowHeight;
				const bigdouble width = g_fFracRight - g_fFracLeft;
				const bigdouble height = g_fFracBottom - g_fFracTop;

				g_fFracLeft += width * xProportion * zoomStrength;
				g_fFracTop += height * yProportion * zoomStrength;
				g_fFracRight -= width * (1.0 - xProportion) * zoomStrength;
				g_fFracBottom -= height * (1.0 - yProportion) * zoomStrength;

				assert(g_fFracRight > g_fFracLeft);

				//  Guesstimate at a new number of iterations. 50*log10(scale)^1.25
				double scale = g_iWindowWidth / (g_fFracRight - g_fFracLeft);
				g_iNumFracIterations = static_cast<int>(g_iNumIterationBase * pow(log10(scale), 1.25));
				g_iNumFracIterations = clamp(g_iNumFracIterations, 50, g_iNumFracIterations);

				bDirty = true;
			}
			else if (lastEvent.type == SDL_MOUSEBUTTONDOWN)
			{
				g_bActiveMouseDragging = true;
			}
			else if (lastEvent.type == SDL_MOUSEMOTION)
			{
				if (g_bActiveMouseDragging)
				{
					g_iActiveMouseDragX += lastEvent.motion.xrel;
					g_iActiveMouseDragY += lastEvent.motion.yrel;
				}
			}
			else if (lastEvent.type == SDL_MOUSEBUTTONUP)
			{
				g_bActiveMouseDragging = false;

				const bigdouble xProportion = -g_iActiveMouseDragX / (bigdouble)g_iWindowWidth;
				const bigdouble yProportion = -g_iActiveMouseDragY / (bigdouble)g_iWindowHeight;
				const bigdouble width = g_fFracRight - g_fFracLeft;
				const bigdouble height = g_fFracBottom - g_fFracTop;

				g_fFracLeft += width * xProportion;
				g_fFracTop += height * yProportion;
				g_fFracRight += width * xProportion;
				g_fFracBottom += height * yProportion;

				g_iActiveMouseDragX = 0;
				g_iActiveMouseDragY = 0;
				bDirty = true;
			}
			else if (lastEvent.type == SDL_QUIT)
			{
				break;
			}
			else if (lastEvent.type == SDL_KEYDOWN)
			{
				if (lastEvent.key.keysym.sym == SDLK_ESCAPE)
				{
					break;
				}
				else if (lastEvent.key.keysym.sym == SDLK_KP_PLUS)
				{
					g_iNumIterationBase++;
					double scale = g_iWindowWidth / (g_fFracRight - g_fFracLeft);
					g_iNumFracIterations = static_cast<int>(g_iNumIterationBase * pow(log10(scale), 1.25));
					g_iNumFracIterations = clamp(g_iNumFracIterations, 50, g_iNumFracIterations);
					bDirty = true;
				}
				else if (lastEvent.key.keysym.sym == SDLK_KP_MINUS)
				{
					g_iNumIterationBase--;
					double scale = g_iWindowWidth / (g_fFracRight - g_fFracLeft);
					g_iNumFracIterations = static_cast<int>(g_iNumIterationBase * pow(log10(scale), 1.25));
					g_iNumFracIterations = clamp(g_iNumFracIterations, 50, g_iNumFracIterations);
					bDirty = true;
				}
				else if (lastEvent.key.keysym.sym == SDLK_r)
				{
					g_fFracLeft = -2.5;
					g_fFracTop = -2.0;
					g_fFracRight = 2.5;
					g_fFracBottom = 2.0;
					bDirty = true;
				}
			}

			if (lastEvent.type == SDL_WINDOWEVENT && (lastEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || lastEvent.window.event == SDL_WINDOWEVENT_RESIZED))
			{
				if (lastEvent.window.data1 > 0 && lastEvent.window.data1 < 4096 && lastEvent.window.data2 > 0 && lastEvent.window.data2 < 4096)
				{
					g_iWindowWidth = lastEvent.window.data1;
					g_iWindowHeight = lastEvent.window.data2;

					SDL_DestroyTexture(g_pMainTexture);
					g_pMainTexture = SDL_CreateTexture(g_pMainRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, g_iWindowWidth, g_iWindowHeight);
					SDL_assert(g_pMainTexture != nullptr);

					//	Fixup invalidated description text
					create_help_text();

					//  Recalc 'bottom' based on the proportionate AR
					bigdouble fOldBottom = g_fFracBottom;

					bigdouble fARtop = g_fFracRight - g_fFracLeft;
					g_fFracBottom = g_fFracTop + fARtop * ((bigdouble)g_iWindowHeight / g_iWindowWidth);

					//  Center the fractal
					g_fFracTop += (fOldBottom - g_fFracBottom) / 2.0;
					g_fFracBottom += (fOldBottom - g_fFracBottom) / 2.0;

					bDirty = true;
				}
			}
		}

		SDL_Delay(10);
	}

	delete[] g_pPixelBackbuffer;
	delete[] g_pWorkingData;

	TTF_CloseFont(g_pMainFont);
	TTF_CloseFont(g_pSmallFont);

	SDL_DestroyTexture(g_pMainTexture);
	g_pMainTexture = nullptr;

	SDL_DestroyTexture(g_pFontTexture);
	g_pFontTexture = nullptr;

	SDL_DestroyTexture(g_pSmallFontTexture);
	g_pSmallFontTexture = nullptr;

	SDL_DestroyWindow(g_pMainWindow);
	g_pMainWindow = nullptr;

	SDL_DestroyRenderer(g_pMainRenderer);
	g_pMainRenderer = nullptr;

	TTF_Quit();
	SDL_Quit();

	return 0;
}
