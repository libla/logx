#include "logx.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#if defined(_WIN32)
	#include <Windows.h>
#else
	#include <sys/time.h>
#endif

namespace
{
	enum
	{
		LEVEL_DEBUG,
		LEVEL_TRACE,
		LEVEL_RECORD,
		LEVEL_WARNING,
		LEVEL_ERROR,
		LEVEL_FATAL,
	};

	enum
	{
		FMT_SLEVEL,
		FMT_FLEVEL,
		FMT_PID,
		FMT_YEAR,
		FMT_TENYEAR,
		FMT_MON,
		FMT_DAY,
		FMT_HOUR,
		FMT_MIN,
		FMT_SEC,
		FMT_USEC,
		FMT_CLK,
		FMT_WORD,

		FTM_NONE
	};

	static const char flags[] = {
		's', 'f', 'p', 'y', 't', 'm', 'd', 'H', 'M', 'S', 'u', 'c', 'w'
	};
}

namespace logx
{
	const int debug = LEVEL_DEBUG;
	const int trace = LEVEL_TRACE;
	const int record = LEVEL_RECORD;
	const int warning = LEVEL_WARNING;
	const int error = LEVEL_ERROR;
	const int fatal = LEVEL_FATAL;

	class default_alloctor : public alloctor
	{
	public:
		virtual void *	take(size_t size)
		{
			return malloc(size);
		}
		virtual void	drop(void *ptr, size_t size)
		{
			free(ptr);
		}
	};

	static default_alloctor _default_alloctor;
	static char _default_prefmt[] = "\x01\x0d \0\x04\x0d-\0\x06\x0d-\0\x07\x0d \0\x08\x0d:\0\x09\x0d:\0\x0a\x0d \0\x0b\x0d \0";
	static char _default_suffmt[] = "";

	sink::sink()
	{
		prev = nullptr;
		next = nullptr;
	}

	sink::~sink()
	{
		if (log->_sinks == this)
		{
			log->_sinks = next;
		}
		if (next != nullptr)
		{
			next->prev = prev;
		}
		if (prev->next != nullptr)
		{
			prev->next = next;
		}
	}

	void sink::start(int level)
	{
	}

	void sink::flush()
	{
	}

	void sink::release()
	{
		logger *log = this->log;
		void *ptr = this->ptr;
		size_t len = this->len;
		{
			std::lock_guard<std::mutex> lock(log->_sinks_mtx);
			this->~sink();
		}
		log->_alloc.drop(ptr, len);
	}

	uninheritable::uninheritable()
	{
	}

	logger::stream::stream(logger &log) : _log(log)
	{
		pos = 0;
		next = nullptr;
	}

	logger::stream * logger::stream::write(char c)
	{
		if (next)
		{
			return next->write(c);
		}
		if (pos + 1 < sizeof(buffer))
		{
			buffer[pos++] = c;
			buffer[pos] = 0;
			return this;
		}
		next = _log.newstream();
		return next->write(c);
	}

	logger::stream * logger::stream::write(const char *s)
	{
		return write(s, strlen(s));
	}

	logger::stream * logger::stream::write(const char *s, size_t l)
	{
		if (next)
		{
			return next->write(s, l);
		}
		if (l < sizeof(buffer))
		{
			if (pos + l < sizeof(buffer))
			{
				memcpy(buffer + pos, s, l);
				pos += l;
				buffer[pos] = 0;
				return this;
			}
			next = _log.newstream();
			return next->write(s, l);
		}
		size_t len = sizeof(buffer) - pos - 1;
		memcpy(buffer + pos, s, len);
		buffer[sizeof(buffer) - 1] = 0;
		next = _log.newstream();
		return next->write(s + len, l - len);
	}

	logger::printer::printer(logger &log, const char *name, int level) : _log(log)
	{
		_name = name;
		_size = _name == nullptr ? 0 : strlen(_name);
		_level = level;
	}

	void logger::printer::nowtime(time_t &sec, int &usec)
	{
#if defined(_WIN32)
		FILETIME ft;
		ULARGE_INTEGER ll;
		GetSystemTimeAsFileTime(&ft);
		ll.LowPart = ft.dwLowDateTime;
		ll.HighPart = ft.dwHighDateTime;
		sec = (time_t)(ll.QuadPart / 10000000 - 11644473600);
		usec = ((int)(ll.QuadPart % 10000000) + 5) / 10;
#else
		struct timeval tv_now;
		gettimeofday(&tv_now, NULL);
		sec = tv_now.tv_sec;
		usec = tv_now.tv_usec;
#endif
	}

	logger::logger() : logger(_default_alloctor) {}

	logger::logger(alloctor &m)
		: _alloc(m)
		, debug(*this, "DEBUG  ", LEVEL_DEBUG)
		, trace(*this, "TRACE  ", LEVEL_TRACE)
		, record(*this, "RECORD ", LEVEL_RECORD)
		, warning(*this, "WARNING", LEVEL_WARNING)
		, error(*this, "ERROR  ", LEVEL_ERROR)
		, fatal(*this, "FATAL  ", LEVEL_FATAL)
	{
		_level = LEVEL_DEBUG;
		_prefmt = _default_prefmt;
		_prelen = 0;
		_suffmt = _default_suffmt;
		_suflen = 0;
		_sinks = nullptr;
		_stream = nullptr;
	}

	logger::logger(logger &&lhs)
		: _alloc(lhs._alloc)
		, debug(*this, "DEBUG  ", LEVEL_DEBUG)
		, trace(*this, "TRACE  ", LEVEL_TRACE)
		, record(*this, "RECORD ", LEVEL_RECORD)
		, warning(*this, "WARNING", LEVEL_WARNING)
		, error(*this, "ERROR  ", LEVEL_ERROR)
		, fatal(*this, "FATAL  ", LEVEL_FATAL)
	{
		_level = lhs._level;
		_prefmt = lhs._prefmt;
		_prelen = lhs._prelen;
		_suffmt = lhs._suffmt;
		_suflen = lhs._suflen;
		_sinks = lhs._sinks;
		_stream = lhs._stream;

		lhs._prefmt = _default_prefmt;
		lhs._prelen = 0;
		lhs._suffmt = _default_suffmt;
		lhs._suflen = 0;
		lhs._sinks = nullptr;
		lhs._stream = nullptr;
	}

	logger::~logger()
	{
		if (_prelen != 0)
		{
			_alloc.drop(_prefmt, _prelen);
		}
		if (_suflen != 0)
		{
			_alloc.drop(_suffmt, _suflen);
		}
		while (_stream)
		{
			stream *old = _stream;
			_stream = _stream->next;
			old->~stream();
			_alloc.drop(old, sizeof(stream));
		}
		{
			std::lock_guard<std::mutex> lock(_sinks_mtx);
			while (_sinks)
			{
				void *ptr = _sinks->ptr;
				size_t len = _sinks->len;
				_sinks->~sink();
				_alloc.drop(ptr, len);
			}
		}
	}

	logger::stream * logger::params<int>::build(stream *os, int i)
	{
		char buffer[22];
		sprintf(buffer, "%d", i);
		return os->write(buffer);
	}

	logger::stream * logger::params<unsigned int>::build(stream *os, unsigned int u)
	{
		char buffer[22];
		sprintf(buffer, "%u", u);
		return os->write(buffer);
	}

	logger::stream * logger::params<const char *>::build(stream *os, const char *s)
	{
		return os->write(s);
	}

	void logger::writefmt(logger::stream *os, const char *fmt, const char *name, size_t size, int level, time_t sec, int usec)
	{
		tm t;
#if defined(_WIN32)
		localtime_s(&t, &sec);
#else
		localtime_r(&sec, &t);
#endif
		char tm_buf[20];
		for (const char *ptr = fmt; *ptr != 0; ++ptr)
		{
			switch (*ptr - 1)
			{
			case FMT_SLEVEL:
				os = os->write(name[0]);
				break;
			case FMT_FLEVEL:
				os = os->write(name, size);
				break;
			case FMT_PID:
				os = os->write('0');
				break;
			case FMT_YEAR:
				strftime(tm_buf, sizeof(tm_buf), "%Y", &t);
				os = os->write(tm_buf);
				break;
			case FMT_TENYEAR:
				strftime(tm_buf, sizeof(tm_buf), "%y", &t);
				os = os->write(tm_buf);
				break;
			case FMT_MON:
				strftime(tm_buf, sizeof(tm_buf), "%m", &t);
				os = os->write(tm_buf);
				break;
			case FMT_DAY:
				strftime(tm_buf, sizeof(tm_buf), "%d", &t);
				os = os->write(tm_buf);
				break;
			case FMT_HOUR:
				strftime(tm_buf, sizeof(tm_buf), "%H", &t);
				os = os->write(tm_buf);
				break;
			case FMT_MIN:
				strftime(tm_buf, sizeof(tm_buf), "%M", &t);
				os = os->write(tm_buf);
				break;
			case FMT_SEC:
				strftime(tm_buf, sizeof(tm_buf), "%S", &t);
				os = os->write(tm_buf);
				break;
			case FMT_USEC:
				sprintf(tm_buf, "%06d", usec);
				os = os->write(tm_buf);
				break;
			case FMT_CLK:
				break;
			case FMT_WORD:
				{
					++ptr;
					size_t len = strlen(ptr);
					os = os->write(ptr, len);
					ptr += len;
				}
				break;
			}
		}
	}

	logger::stream * logger::newstream()
	{
		stream *init;
		if (_stream != nullptr)
		{
			std::lock_guard<std::mutex> lock(_stream_mtx);
			init = _stream;
			_stream = _stream->next;
			init->~stream();
		}
		else
		{
			init = (stream *)_alloc.take(sizeof(stream));
		}
		new (init)stream(*this);
		return init;
	}

	logger::stream * logger::prepare(const char *name, size_t size, int level, time_t sec, int usec)
	{
		stream *init = newstream();
		writefmt(init, _prefmt, name, size, level, sec, usec);
		return init;
	}

	void logger::flush(logger::stream *init, const char *name, size_t size, int level, time_t sec, int usec)
	{
		stream *os = init;
		while (os->next != nullptr)
		{
			os = os->next;
		}
		writefmt(os, _suffmt, name, size, level, sec, usec);
		work(level, init);
	}

	void logger::work(int level, stream *init)
	{
		sink *s = _sinks;
		while (s)
		{
			s->start(level);
			stream *is = init;
			while (is)
			{
				s->write(is->buffer, is->pos);
				is = is->next;
			}
			s->flush();
			s = s->next;
		}
		{
			std::lock_guard<std::mutex> lock(_stream_mtx);
			stream *is = init;
			while (is)
			{
				stream *old = _stream;
				_stream = is;
				is = is->next;
				_stream->next = old;
			}
		}
	}

	logger & logger::level(printer &print)
	{
		return level(print._level);
	}

	logger & logger::level(int level)
	{
		_level = level;
		return *this;
	}

	static void format(alloctor &alloc, const char *fmt, char *&nfmt, size_t &nlen)
	{
		nlen = 1;
		int last;
		last = FTM_NONE;
		for (const char *ptr = fmt; *ptr != 0; ++ptr)
		{
			if (*ptr == '%')
			{
				char c = *++ptr;
				if (c == '%')
				{
					if (last != FMT_WORD)
					{
						++nlen;
						last = FMT_WORD;
					}
					++nlen;
					continue;
				}
				else if (c == 0)
				{
					break;
				}
				int index = -1;
				for (int i = 0; i < sizeof(flags); ++i)
				{
					if (flags[i] == c)
					{
						index = i;
						break;
					}
				}
				if (index >= 0)
				{
					if (last == FMT_WORD)
					{
						++nlen;
					}
					++nlen;
					last = index;
				}
				continue;
			}
			if (last != FMT_WORD)
			{
				++nlen;
				last = FMT_WORD;
			}
			++nlen;
		}
		if (last == FMT_WORD)
		{
			++nlen;
		}
		nfmt = (char *)alloc.take(nlen);
		char *_ptr = nfmt;
		last = FTM_NONE;
		for (const char *ptr = fmt; *ptr != 0; ++ptr)
		{
			if (*ptr == '%')
			{
				char c = *++ptr;
				if (c == '%')
				{
					if (last != FMT_WORD)
					{
						*_ptr++ = FMT_WORD + 1;
						last = FMT_WORD;
					}
					*_ptr++ = '%';
					continue;
				}
				else if (c == 0)
				{
					break;
				}
				int index = -1;
				for (int i = 0; i < sizeof(flags); ++i)
				{
					if (flags[i] == c)
					{
						index = i;
						break;
					}
				}
				if (index >= 0)
				{
					if (last == FMT_WORD)
					{
						*_ptr++ = 0;
					}
					*_ptr++ = index + 1;
					last = index;
				}
				continue;
			}
			if (last != FMT_WORD)
			{
				*_ptr++ = FMT_WORD + 1;
				last = FMT_WORD;
			}
			*_ptr++ = *ptr;
		}
		if (last == FMT_WORD)
		{
			*_ptr++ = 0;
		}
		*_ptr = 0;
	}

	logger & logger::prefix(const char *fmt)
	{
		if (_prelen != 0)
		{
			_alloc.drop(_prefmt, _prelen);
		}
		format(_alloc, fmt, _prefmt, _prelen);
		return *this;
	}

	logger & logger::suffix(const char *fmt)
	{
		if (_suflen != 0)
		{
			_alloc.drop(_suffmt, _suflen);
		}
		format(_alloc, fmt, _suffmt, _suflen);
		return *this;
	}

	void logger::add(sink &s)
	{
		if (s.prev == nullptr && s.next == nullptr)
		{
			std::lock_guard<std::mutex> lock(_sinks_mtx);
			if (_sinks == nullptr)
			{
				_sinks = &s;
				_sinks->prev = _sinks;
			}
			else
			{
				_sinks->prev->next = &s;
				s.prev = _sinks->prev;
				_sinks->prev = &s;
			}
		}
	}
}
