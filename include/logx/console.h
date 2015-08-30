#pragma once
#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "logx.h"

namespace logx
{
	class console : public sink
	{
	public:
		console();
		virtual ~console();
		virtual void start(int level);
		virtual void write(const char *text, size_t len);
		virtual void flush();

	private:
		bool mustfree;
	};
}

#endif