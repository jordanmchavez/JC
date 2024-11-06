// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSCommon/Test.h"
#include <vector>

namespace EOSC
{
	std::vector<int> Run;

	// This setup (record then verify) only works if the tests are not shuffled
	// If we ever add shuffling (which is a good idea), then we need to rework this
	TEST("Test.Record subtest execution")
	{
		Run.emplace_back(0);
		SUBTEST("1")
		{
			Run.emplace_back(1);
			SUBTEST("2")
			{
				Run.emplace_back(2);
				SUBTEST("3")
				{
					Run.emplace_back(3);
				}
				SUBTEST("4")
				{
					Run.emplace_back(4);
				}
				SUBTEST("5")
				{
					Run.emplace_back(5);
				}
				SUBTEST("6")
				{
					Run.emplace_back(6);
				}
			}
			SUBTEST("7")
			{
				Run.emplace_back(7);
				SUBTEST("8") 
				{
					Run.emplace_back(8);
				}
				SUBTEST("9")
				{
					Run.emplace_back(9);
				}
			}
		}
		SUBTEST("10")
		{
			Run.emplace_back(10);
		}
	}

	TEST("Test.Verify subtest recording")
	{
		CHECK(Run == (std::vector<int> {
			0, 1, 2, 3,
			0, 1, 2, 4,
			0, 1, 2, 5,
			0, 1, 2, 6,
			0, 1, 7, 8,
			0, 1, 7, 9,
			0, 10
		}));
	}
}