#pragma once

#include "JC/Bit.h"
#include "JC/UnitTest.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

TEST("Bit") {
	CHECK_EQ(Bsf64(0),                  0u);
	CHECK_EQ(Bsr64(0),                  0u);
	CHECK_EQ(Bsf64(1),                  0u);
	CHECK_EQ(Bsr64(1),                  0u);
	CHECK_EQ(Bsf64(0x80000000),         31u);
	CHECK_EQ(Bsf64(0x80008000),         15u);
	CHECK_EQ(Bsr64(0x80000008),         31u);
	CHECK_EQ(Bsr64(0x7fffffff),         30u);
	CHECK_EQ(Bsr64(0x80000000),         31u);
	CHECK_EQ(Bsr64(0x100000000),        32u);
	CHECK_EQ(Bsr64(0xffffffffffffffff), 63u);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC