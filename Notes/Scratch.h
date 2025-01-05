#include "JC/Common.h"

#include "JC/Array.h"
#include "JC/Math.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

sprite handle = sprite sheet + sprite ID within sheet;
sprites ordered by spritesheet
[0] -> sprites...
[1] -> sprites...
[2] -> sprites...;

hierarchical grid of sprites, only render current + neighboring tiles
we have a fixed-size world so this can be hardcoded;

struct SpriteComponents {
	

};

struct ParticleComponents {
};

//--------------------------------------------------------------------------------------------------

/*

Dim |   16 |  32 | Size
----+------+-----+-----
256 |  256 |  64 | 256k
512 |   1k | 256 |   1m
 1k |   4k |  1k |   4m
 2k |  16k |  4k |  16m
 4k |  64k | 16k |  64m
 8k | 256k | 64k | 256m

 */

}	// namespace JC