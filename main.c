#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>

HWND main_window;

LPWSTR filename;
LPWSTR info_filename;
LPWSTR info_temp_filename;

LPWSTR code_prefix;

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
	{14, L"Bouncy Blocks"},
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
		if ((*sub)[i] > (*sup)[i])
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

static void load_db(void)
{
	FILE *f;
	struct stat st;

	f = _wfopen(info_filename, L"rb");
	if (!f)
		return;
	fstat(_fileno(f), &st);
	global_infos.len = global_infos.size = st.st_size / sizeof(permutation_info);
	if (global_infos.len)
	{
		global_infos.items = malloc(st.st_size);
		fread(global_infos.items, sizeof(permutation_info), global_infos.len, f);
	}
	fclose(f);
}

static void save_db(void)
{
	FILE *f;

	f = _wfopen(info_temp_filename, L"wb");
	fwrite(global_infos.items, sizeof(permutation_info), global_infos.len, f);
	fclose(f);

	if (!ReplaceFileW(info_filename, info_temp_filename, NULL, 0, NULL, NULL))
		MoveFile(info_temp_filename, info_filename);
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

	assert(*min_difficulty <= *max_difficulty);
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

		while (candidate[obstacle] > 0)
		{
			double prev_min = *min_difficulty;

			candidate[obstacle]--;

			get_difficulty_range(&candidate, min_difficulty, max_difficulty);

			if (*max_difficulty < *desired_difficulty)
			{
				// Desired difficulty is in a discontinuity, get as close as we can
				if (prev_min - *desired_difficulty < *desired_difficulty - *max_difficulty)
					candidate[obstacle]++;
				memcpy(result, candidate, sizeof(candidate));
				return;
			}

			if (*min_difficulty < *desired_difficulty)
			{
				// We've lowered it to an acceptable level
				memcpy(result, candidate, sizeof(candidate));
				return;
			}
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
	current_level_info.seed = (int)randint(0, 2147483647);
	current_level_info.location = locations[randint(0, sizeof(locations) / sizeof(locations[0]) - 1)];
}

static BOOL is_consistent(const permutation_info *a, const permutation_info *b)
{
	BOOL a_subset_b, b_subset_a;
	double a_difficulty, b_difficulty;

	a_subset_b = is_subset(&a->permutation, &b->permutation);
	b_subset_a = is_subset(&b->permutation, &a->permutation);

	if (!a_subset_b && !b_subset_a)
		return TRUE;

	a_difficulty = a->difficulty_numerator / (double)a->difficulty_denominator;
	b_difficulty = b->difficulty_numerator / (double)b->difficulty_denominator;

	if (a_subset_b && a_difficulty > b_difficulty)
		return FALSE;

	if (b_subset_a && b_difficulty > a_difficulty)
		return FALSE;

	return TRUE;
}

static void update_info(permutation *p, int difficulty_numerator, int difficulty_denominator)
{
	int i;
	permutation_info new_info;

	for (i = 0; i < global_infos.len; i++)
	{
		if (memcmp(&global_infos.items[i].permutation, p, sizeof(permutation)) == 0)
			break;
	}

	memcpy(new_info.permutation, *p, sizeof(new_info.permutation));

	if (i < global_infos.len)
	{
		new_info.difficulty_denominator = global_infos.items[i].difficulty_denominator + difficulty_denominator;
		new_info.difficulty_numerator = global_infos.items[i].difficulty_numerator + difficulty_numerator;
		global_infos.items[i] = global_infos.items[global_infos.len - 1];
		global_infos.len--;
	}
	else
	{
		new_info.difficulty_denominator = difficulty_denominator;
		new_info.difficulty_numerator = difficulty_numerator;
	}

	// Remove anything inconsistent with this info
	i = 0;
	while (i < global_infos.len)
	{
		if (is_consistent(&new_info, &global_infos.items[i]))
			i++;
		else
		{
			global_infos.items[i] = global_infos.items[global_infos.len - 1];
			global_infos.len--;
		}
	}

	// Add the new info
	append_info(&global_infos, &new_info);

	save_db();
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
	wchar_t *result;
	wchar_t *settings;
	int i;
	settings = strdup_wprintf(L"%g", current_level_info.permutation[0] / (double)MAX_OBSTACLE_SETTING * 9.0);
	for (i = 1; i < NUM_OBSTACLES; i++)
	{
		wchar_t *new_settings = strdup_wprintf(L"%s,%g", settings, current_level_info.permutation[i] / (double)MAX_OBSTACLE_SETTING * 9.0);
		free(settings);
		settings = new_settings;
	}
	result = strdup_wprintf(L"%s;s:%i;t:%s;u:%s", code_prefix, current_level_info.seed, current_level_info.location, settings);
	return result;
}

static wchar_t* get_readable_settings(const permutation *p)
{
	wchar_t *result = _wcsdup(L"");
	int i;

	for (i = 0; i < sizeof(obstacles) / sizeof(obstacles[0]); i++)
	{
		int index = obstacles[i].index;
		if ((*p)[index] != 0)
		{
			wchar_t *new_result;
			new_result = strdup_wprintf(L"%s%s: %.0f%%\n", result, obstacles[i].name, (*p)[index] / (double)MAX_OBSTACLE_SETTING * 100);
			free(result);
			result = new_result;
		}
	}

	return result;
}

static double get_mastery(void)
{
	permutation mastered = { 0 };
	int i, j;
	permutation_info *info;
	int result=0;

	for (i = 0; i < global_infos.len; i++)
	{
		info = &global_infos.items[i];
		if (info->difficulty_numerator * 6 < info->difficulty_denominator)
		{
			for (j = 0; j < NUM_OBSTACLES; j++)
			{
				if (info->permutation[j] > mastered[j])
					mastered[j] = info->permutation[j];
			}
		}
	}

	for (j = 0; j < NUM_OBSTACLES; j++)
		result += mastered[j];

	return (double)result / (sizeof(obstacles) / sizeof(obstacles[0]) * MAX_OBSTACLE_SETTING);
}

static void update_gui()
{
	HANDLE clipdata;
	wchar_t *level_code;
	void *lock;

	InvalidateRect(main_window, NULL, TRUE);

	level_code = get_level_code();
	clipdata = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * (wcslen(level_code) + 1));
	lock = GlobalLock(clipdata);
	wcscpy(lock, level_code);
	GlobalUnlock(clipdata);
	free(level_code);

	OpenClipboard(main_window);
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, clipdata);
	CloseClipboard();
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
		wchar_t *output, *level_code, *readable_settings;
		RECT client;
		int save;

		hdc = BeginPaint(hwnd, &ps);

		save = SaveDC(hdc);

		level_code = get_level_code();

		readable_settings = get_readable_settings(&current_level_info.permutation);

		output = strdup_wprintf(L"Level code:\n%s\n\nDesired difficulty: %.0f%%\nExpected difficulty: %.0f-%.0f%%\n\n%s\nMastery: %.0f%%", level_code,
			current_level_info.desired_difficulty*100, current_level_info.min_difficulty*100, current_level_info.max_difficulty*100,
			readable_settings,
			get_mastery() * 100);

		GetClientRect(hwnd, &client);

		SetBkMode(hdc, TRANSPARENT);

		SetTextColor(hdc, RGB(80, 150, 255));

		DrawTextW(hdc, output, -1, &client, DT_LEFT | DT_NOPREFIX | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOCLIP);

		free(output);

		free(level_code);

		free(readable_settings);

		RestoreDC(hdc, save);

		EndPaint(hwnd, &ps);

		break;
	}
	case WM_KEYDOWN:
	{
		switch (wparam)
		{
		case '1':
			update_info(&current_level_info.permutation, 3, 3);
			generate_level();
			update_gui();
			break;
		case '2':
			update_info(&current_level_info.permutation, 2, 3);
			generate_level();
			update_gui();
			break;
		case '3':
			update_info(&current_level_info.permutation, 1, 3);
			generate_level();
			update_gui();
			break;
		case '4':
			update_info(&current_level_info.permutation, 0, 3);
			generate_level();
			update_gui();
			break;
		}

		break;
	}
	case WM_HOTKEY:
	{
		switch (wparam)
		{
		case 1:
			update_info(&current_level_info.permutation, 3, 3);
			generate_level();
			update_gui();
			break;
		case 2:
			update_info(&current_level_info.permutation, 2, 3);
			generate_level();
			update_gui();
			break;
		case 3:
			update_info(&current_level_info.permutation, 1, 3);
			generate_level();
			update_gui();
			break;
		case 4:
			update_info(&current_level_info.permutation, 0, 3);
			generate_level();
			update_gui();
			break;
		}

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
	cls.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
	cls.lpszClassName = L"main";

	RegisterClassEx(&cls);
}

static HWND create_mainwindow(void)
{
	register_class();

	main_window = CreateWindowEx(0, L"main", L"cktrain", WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		200, 720, NULL, NULL, NULL, NULL);

	return main_window;
}

static wchar_t* read_ascii_file(LPCWSTR filename)
{
	char buffer[4096];
	size_t len;
	FILE *f;

	f = _wfopen(filename, L"r");
	len = fread(buffer, 1, sizeof(buffer) - 1, f);
	fclose(f);

	buffer[len] = '\0';
	while (buffer[len - 1] == '\r' || buffer[len - 1] == '\n')
		buffer[--len] = '\0';

	return strdup_wprintf(L"%S", buffer);
}

static void register_hotkeys(void)
{
	RegisterHotKey(main_window, 1, MOD_NOREPEAT, VK_F1);
	RegisterHotKey(main_window, 2, MOD_NOREPEAT, VK_F2);
	RegisterHotKey(main_window, 3, MOD_NOREPEAT, VK_F3);
	RegisterHotKey(main_window, 4, MOD_NOREPEAT, VK_F4);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int argc;
	LPWSTR* argv;

	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argc < 2)
	{
		MessageBoxW(NULL, L"A filename is required", L"cktrain", MB_ICONERROR|MB_OK);
		return 0;
	}

	filename = argv[1];
	info_filename = strdup_wprintf(L"%s.db", filename);
	info_temp_filename = strdup_wprintf(L"%s.db.tmp", filename);

	code_prefix = read_ascii_file(filename);

	load_db();

	create_mainwindow();

	generate_level();

	update_gui();

	register_hotkeys();

	loop();

	return 0;
}