namespace EOSC::Test
{
	int Run(const char* CmdLine);
	int Run(int Argc, const char* const* Argv);
}

// Consume() nonsense is just to avoid compiler warnings about unused vars
#define EOSC_TEST_IMPL(Name, Fn, Var) \
	static void Fn(); \
	static const int Var = ::EOSC::Test::Consume( \
		&Var, \
		::EOSC::Test::Register(Name, &Fn, __FILE__, __LINE__) \
	); \
	static void Fn()

#define TEST(Name) EOSC_TEST_IMPL(Name, EOSC_UNIQUE_NAME(EOSC_TEST_ANON_FUNC_), EOSC_UNIQUE_NAME(EOSC_TEST_ANON_VAR_) )

#define SUBTEST(Name) if (::EOSC::Test::FSubtest EOSC_UNIQUE_NAME(EOSC_TEST_ANON_SUBTEST_) = ::EOSC::Test::FSubtest(Name, __FILE__, __LINE__))
