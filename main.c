#include <windows.h>
#include <stdio.h>
#include <assert.h>

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

struct {
	permutation permutation;
	double desired_difficulty;
	double min_difficulty;
	double max_difficulty;
	int seed;
	const wchar_t *location;
} current_level_info;

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

static BOOL is_subset(const permutation* sub, const permutation* sup)
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

static LONGLONG randint(LONGLONG lower, LONGLONG upper)
{
	LONGLONG range = upper - lower;
	LONGLONG gen_upper;
	LONGLONG result;

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

static double rand_double()
{
	return randint(0, 9007199254740991) / 9007199254740992.0;
}

static void get_difficulty_range(permutation *result, double *min_difficulty, double *max_difficulty)
{
	int i;

	*min_difficulty = 0.0;
	*max_difficulty = 1.0;

	for (i = 0; i < global_infos.len; i++)
	{
		permutation_info *info = &global_infos.items[i];
		if (is_subset(result, &info->permutation))
		{
			double difficulty = info->difficulty_numerator / (double)info->difficulty_denominator;
			if (difficulty < *max_difficulty)
				*max_difficulty = difficulty;
		}
		if (is_subset(&info->permutation, result))
		{
			double difficulty = info->difficulty_numerator / (double)info->difficulty_denominator;
			if (difficulty > *min_difficulty)
				*min_difficulty = difficulty;
		}
	}

	assert(*min_difficulty < *max_difficulty);
}

static void choose_permutation(double *desired_difficulty, double *min_difficulty, double *max_difficulty, permutation *result)
{
	permutation candidate = { 0 };
	static const int num_obstacles = sizeof(obstacles) / sizeof(obstacles[0]);
	int remaining_obstacles[sizeof(obstacles) / sizeof(obstacles[0])];
	int i;

	*desired_difficulty = rand_double();

	get_difficulty_range(&candidate, min_difficulty, max_difficulty);

	if (*min_difficulty <= *desired_difficulty && *desired_difficulty <= *max_difficulty)
		return;

	for (i = 0; i < num_obstacles; i++)
		remaining_obstacles[i] = i;

	for (i = 0; i < num_obstacles; i++)
	{
		int n = (int)randint(i, num_obstacles - 1);
		int obstacle = obstacles[remaining_obstacles[n]].index;
		remaining_obstacles[n] = remaining_obstacles[i];

		candidate[obstacle] = (unsigned char)randint(1, MAX_OBSTACLE_SETTING);

		get_difficulty_range(&candidate, min_difficulty, max_difficulty);

		// if this configuration is too easy, add another obstacle
		if (*max_difficulty < *desired_difficulty)
			continue;

		if (*min_difficulty <= *desired_difficulty)
			// the range includes our desired difficulty, good
			break;

		while (candidate[obstacle] > 1)
		{
			double prev_max = *max_difficulty;

			candidate[obstacle]--;

			get_difficulty_range(&candidate, min_difficulty, max_difficulty);

			if (*max_difficulty < *desired_difficulty)
			{
				// Desired difficulty is in a discontinuity, get as close as we can
				if (prev_max - *desired_difficulty < *desired_difficulty - *max_difficulty)
					candidate[obstacle]++;
				memcpy(result, candidate, sizeof(candidate));
				return;
			}

			if (*min_difficulty < *desired_difficulty)
				// We've lowered it to an acceptable level
				memcpy(result, candidate, sizeof(candidate));
			return;
		}
	}

	memcpy(result, candidate, sizeof(candidate));
}

static void generate_level(void)
{
	static const wchar_t* locations[] = { L"cloud", L"cave", L"castle", L"forest", L"sea", L"hills" };

	choose_permutation(&current_level_info.desired_difficulty,
		&current_level_info.min_difficulty,
		&current_level_info.max_difficulty,
		&current_level_info.permutation);
	current_level_info.seed = randint(0, 2147483647);
	current_level_info.location = locations[randint(0, sizeof(locations) / sizeof(locations[0]) - 1)];
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

static wchar_t* get_level_code()
{
	static const wchar_t *code_prefix = L"CODE_PREFIX";
	wchar_t *result;
	wchar_t *settings;
	int i;
	settings = strdup_wprintf(L"%g", current_level_info.permutation[0] / (double)MAX_OBSTACLE_SETTING);
	for (i = 1; i < sizeof(obstacles) / sizeof(obstacles[0]); i++)
	{
		wchar_t *new_settings = strdup_wprintf(L"%s,%g", settings, current_level_info.permutation[0] / (double)MAX_OBSTACLE_SETTING);
		free(settings);
		settings = new_settings;
	}
	result = strdup_wprintf(L"%s;s:%i;t:%s;u:%s", code_prefix, current_level_info.seed, current_level_info.location, settings);
	return result;
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
		wchar_t *output, *level_code;
		RECT client;
		int save;

		hdc = BeginPaint(hwnd, &ps);

		save = SaveDC(hdc);

		level_code = get_level_code();

		output = strdup_wprintf(L"Level code:\n%s\n\nDifficulty: %.1f%%", level_code, rand_double() * 100.0);

		GetClientRect(hwnd, &client);

		SetBkMode(hdc, TRANSPARENT);

		DrawTextW(hdc, output, -1, &client, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOCLIP);

		free(output);

		free(level_code);

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

	// load data

	generate_level();

	loop();

	return 0;
}