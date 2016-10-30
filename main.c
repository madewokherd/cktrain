#include <windows.h>
#include <stdio.h>

typedef struct obstacle {
	int index;
	const wchar_t *name;
} obstacle;

const struct obstacle obstacles[] = {
	{0, L"Fireballs"},
	//{1, L"Fire Rings"},
	{3, L"Spikes"},
	{4, L"Falling Blocks"},
	{5, L"Flying Blobs"},
	{6, L"Firespinners"},
	{7, L"Moving Blocks"},
	{8, L"Elevators"},
	{9, L"Boulders"},
	{10, L"Spikey Guys"},
	{11, L"Spikey Line"},
	{12, L"Lasers"},
	{13, L"Ghost Blocks"},
	{14, L"Bouncy BLocks"},
	{15, L"Clouds"},
	//{16, L"Conveyors"},
	{17, L"Pendulums"},
	{18, L"Serpent"},
	{19, L"Sludge"},
	{21, L"Level Speed"},
	{22, L"Jump Difficulty"},
	{23, L"Ceilings"},
};

#define NUM_OBSTACLES 24
#define MAX_OBSTACLE_SETTING 32

typedef unsigned char permutation[NUM_OBSTACLES];

typedef struct permutation_info {
	permutation permutation;
	int difficulty_numerator;
	int difficulty_denominator;
} permutation_info;

typedef struct permutation_infos {
	permutation_info *items;
	int len;
	int size;
} permutation_infos;

permutation_infos global_infos;

static void append_info(permutation_infos *list, const permutation_info *item)
{
	if (list->len == list->size)
	{
		if (list->size)
			list->size = list->size * 2;
		else
			list->size = 512;
		list->items = realloc(list->items, list->size * sizeof(*list->items));
	}
	list->items[list->len++] = *item;
}

static void free_infos(permutation_infos *list)
{
	free(list->items);
	list->items = NULL;
	list->len = list->size = 0;
}

static BOOL is_superset(const permutation* sub, const permutation* sup)
{
	int i;

	for (i = 0; i < NUM_OBSTACLES; i++)
	{
		if (sub[i] > sup[i])
			return FALSE;
	}

	return TRUE;
}

static void get_random_bits(char* output, int output_len)
{
	static HCRYPTPROV hprov;

	if (!hprov)
	{
		CryptAcquireContextW(&hprov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
	}

	CryptGenRandom(hprov, output_len, output);
}

static int get_random_bit()
{
	static char bits;
	static char next_bit = 0;
	int result;

	if (!next_bit)
	{
		get_random_bits(&bits, 1);
		next_bit = 1;
	}

	result = (bits & next_bit) != 0;

	next_bit = (next_bit << 1) & 0xff;

	return result;
}

static int randint(int lower, int upper)
{
	int range = upper - lower;
	int gen_upper;
	int result;

	do {
		gen_upper = 1;
		result = 0;
		while (gen_upper <= range)
		{
			gen_upper <<= 1;
			result = (result << 1) | get_random_bit();
		}
	} while (result > range);

	return result + lower;
}

static wchar_t* strdup_wprintf(const wchar_t *format, ...)
{
	va_list args;
	wchar_t *str;
	int len;

	va_start(args, format);

	len = _vscwprintf(format, args);

	str = malloc(sizeof(wchar_t) * (len + 1));

	_vswprintf(str, format, args);

	va_end(args);

	return str;
}

static LRESULT CALLBACK main_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc;
		wchar_t *output;
		RECT client;
		int save;

		hdc = BeginPaint(hwnd, &ps);

		save = SaveDC(hdc);

		output = strdup_wprintf(L"Level code:\n%s\n\nDifficulty: %.1f%%", L"LEVELCODE", randint(0,999) / 10.0);

		GetClientRect(hwnd, &client);

		SetBkMode(hdc, TRANSPARENT);

		DrawTextW(hdc, output, -1, &client, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK);

		free(output);

		RestoreDC(hdc, save);

		EndPaint(hwnd, &ps);

		break;
	}
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void loop(void)
{
	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static void register_class(void)
{
	WNDCLASSEX cls;

	ZeroMemory(&cls, sizeof(cls));
	cls.cbSize = sizeof(cls);
	cls.style = 0;
	cls.lpfnWndProc = main_wndproc;
	cls.hInstance = NULL;
	cls.hCursor = LoadCursor(0, IDC_ARROW);
	cls.hbrBackground = (HBRUSH)COLOR_WINDOW;
	cls.lpszClassName = L"main";

	RegisterClassEx(&cls);
}

static HWND create_mainwindow(void)
{
	register_class();

	return CreateWindowEx(0, L"main", L"cktrain", WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		200, 720, NULL, NULL, NULL, NULL);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	create_mainwindow();

	loop();

	return 0;
}