#pragma once
#ifndef __LOGX_H__
#define __LOGX_H__

#include <time.h>
#include <new>

namespace logx
{
	extern const int debug;
	extern const int trace;
	extern const int record;
	extern const int warning;
	extern const int error;
	extern const int fatal;

	class logger;

	class alloctor
	{
	public:
		virtual void *	take(size_t size) = 0;
		virtual void	drop(void *ptr, size_t size) = 0;
	};

	class sink
	{
		friend class logger;
	public:
		sink();
		virtual ~sink();
		virtual void start(int level);
		virtual void write(const char *text, size_t len) = 0;
		virtual void flush();

	public:
		void release();

	private:
		sink *prev;
		sink *next;
		logger *log;
		void *ptr;
		size_t len;
	};

	class uninheritable
	{
		friend class logger;

	private:
		uninheritable();
	};

	class logger : private virtual uninheritable
	{
	private:
		class stream
		{
		public:
			stream(logger &log);
			stream * write(char c);
			stream * write(const char *s);
			stream * write(const char *s, size_t l);

		public:
			logger &_log;
			char buffer[4096];
			int pos;
			stream *next;
		};

	public:
		class printer
		{
			friend class logger;
		private:
			printer(logger &log, const char *name, int level);
			static void nowtime(time_t &sec, int &usec);

		public:
			template <typename... Args>
			void operator () (Args... args)
			{
				if (_log._level <= _level && _log._sinks != nullptr)
				{
					time_t sec;
					int usec;
					nowtime(sec, usec);
					stream *init = _log.prepare(_name, _size, _level, sec, usec);
					stream *os = init;
					while (os->next != nullptr)
					{
						os = os->next;
					}
					_log.build(os, args...);
					_log.flush(init, _name, _size, _level, sec, usec);
				}
			}

			template <typename... Args>
			void operator () (bool condition, Args... args)
			{
				if (condition)
					return *this(args...);
			}

		private:
			logger &_log;
			const char *_name;
			size_t _size;
			int _level;
		};

		friend class printer;
		friend class sink;
		friend class stream;

	public:
		printer debug;
		printer trace;
		printer record;
		printer warning;
		printer error;
		printer fatal;

	public:
		logger();
		logger(alloctor &m);
		~logger();

	private:
		template <typename T>
		class params;

		template <typename T, typename... Args>
		void build(stream *os, T t, Args... args)
		{
			build(params<T>::build(os, t), args...);
		}

		template <typename T>
		void build(stream *os, T t)
		{
			params<T>::build(os, t);
		}

		static void writefmt(logger::stream *os, const char *fmt, const char *name, size_t size, int level, time_t sec, int usec);
		stream * newstream();
		stream * prepare(const char *name, size_t size, int level, time_t sec, int usec);
		void flush(stream *init, const char *name, size_t size, int level, time_t sec, int usec);

		void work(int level, stream *init);

	public:
		logger & level(printer &print);
		logger & level(int level);
		logger & prefix(const char *fmt);
		logger & suffix(const char *fmt);

		template <typename T, typename... Args>
		T * create(Args... args)
		{
			T *t = (T *)_alloc.take(sizeof(T));
			new (t)T(args...);
			t->log = this;
			t->ptr = t;
			t->len = sizeof(T);
			add(*t);
			return t;
		}

	private:
		void add(sink &s);

	private:
		alloctor &_alloc;
		int _level;
		char *_prefmt;
		size_t _prelen;
		char *_suffmt;
		size_t _suflen;

		sink *_sinks;
		stream *_stream;
	};

	template <>
	class logger::params<int>
	{
	public:
		static stream * build(stream *os, int i);
	};
	template <>
	class logger::params<unsigned int>
	{
	public:
		static stream * build(stream *os, unsigned int u);
	};
	template <>
	class logger::params<const char *>
	{
	public:
		static stream * build(stream *os, const char *s);
	};
}

#endif
