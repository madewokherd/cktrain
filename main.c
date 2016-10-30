#include <windows.h>
#include <stdio.h>

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

		output = strdup_wprintf(L"Level code:\n%s\n\nDifficulty: %.1f%%", L"LEVELCODE", 99.9);

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