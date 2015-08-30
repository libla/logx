#include <cstdio>
#include <gtest/gtest.h>
#include <logx/logx.h>
#include <logx/console.h>

TEST(logx, console)
{
	logx::logger log = logx::logger();
	log.prefix("%s %y-%m-%d %H:%M:%S.%u ***** ").suffix(" *****")
		.create<logx::console>();
	log.debug("Hello, debug!");
	log.trace("Hello, trace!");
	log.record("Hello, record!");
	log.warning("Hello, warning!");
	log.error("Hello, error!");
	log.fatal("Hello, fatal!");
}