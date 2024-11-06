#pragma once

#include "JC/Common.h"

namespace JC {

struct TestApi {
	virtual void Run() = 0;
	virtual void CheckFail() = 0;
};

using TestFn = void (TestApi*);

struct TestRegistrar {
	TestRegistrar(s8 name, TestFn* fn);
};

struct Subtest {
	s8  name;
	s8  file;
	i32 line;

	Subtest(s8 name);
	~Subtest();
	operator bool() const;
};

#define TEST(name) \
	JC_TEST_IMPL(name, JC_MACRO_NAME(JC_Test_Fn), JC_MACRO_NAME(JC_Test_Registrar))

#define JC_TEST_IMPL(name, fn, registrar)                         \
	static void fn(TestApi* testApi);                             \
	static JC::TestRegistrar registrar = TestRegistrar(name, fn); \
	static void fn(TestApi* testApi)

#define SUBTEST(name) \
	if (::JC::Subtest JC_MACRO_NAME(JC_Subtest) = ::JC::Subtest(name, __FILE__, __LINE__))

#define CHECK(x)
#define CHECK_EQ(x, y)

}	// namespace JC