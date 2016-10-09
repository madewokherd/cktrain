#include <windows.h>
#include <stdio.h>

static LRESULT CALLBACK main_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
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