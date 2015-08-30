#include "console.h"
#include <stdio.h>
#ifdef _WIN32
	#include <Windows.h>
#endif

namespace logx
{
	static int loglevel;
#ifdef _WIN32
	console::console()
	{
		mustfree = false;
		if (AllocConsole())
		{
			mustfree = true;
			freopen("CONIN$", "rt", stdin);
			freopen("CONOUT$", "wt", stdout);
			freopen("CONOUT$", "wt", stderr);
		}
	}

	console::~console()
	{
		if (mustfree)
		{
			FreeConsole();
		}
	}

	static WORD wOldColorAttrs;

	void console::start(int level)
	{
		loglevel = level;
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
		GetConsoleScreenBufferInfo(h, &csbiInfo);
		wOldColorAttrs = csbiInfo.wAttributes;

		if (level == fatal)
		{
			SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		}
		else if (level == error)
		{
			SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_INTENSITY);
		}
		else if (level == warning)
		{
			SetConsoleTextAttribute(h, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		}
		else if (level == debug)
		{
			SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		}
		else
		{
			SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		}
	}

	void console::flush()
	{
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(h, wOldColorAttrs);
		if (loglevel >= error)
			fprintf(stderr, "\n");
		else
			fprintf(stdout, "\n");
	}
#else
	console::console()
	{
	}

	console::~console()
	{
	}

	void console::start(int level)
	{
		loglevel = level;
		if (level == fatal)
		{
			fprintf(stderr, "\033[01;35m");
		}
		else if (level == error)
		{
			fprintf(stderr, "\033[01;31m");
		}
		else if (level == warning)
		{
			fprintf(stdout, "\033[01;36m");
		}
		else if (level == debug)
		{
			fprintf(stdout, "\033[01;33m");
		}
		else
		{
			fprintf(stdout, "\033[01;37m");
		}
	}

	void console::flush()
	{
		if (loglevel >= error)
			fprintf(stderr, "\033[0m\n");
		else
			fprintf(stdout, "\033[0m\n");
	}
#endif

	void console::write(const char *text, size_t len)
	{
		if (loglevel >= error)
			fprintf(stderr, "%s", text);
		else
			fprintf(stdout, "%s", text);
	}
}
