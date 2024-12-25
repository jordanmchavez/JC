#include "JC/Common.h"

#include "JC/Array.h"
#include "JC/Math.h"

namespace JC {

//--------------------------------------------------------------------------------------------------


/*
machinery model is that command buffers are allocated lazily *at submit time*
submitted command buffers are then marked as in-flight, and we have a regular check for recycling done once per frame
this decouples command buffer management from frames
however all command buffer submits must still wait on the same semaphore structure

an even simpler approach is to always have a command buffer open and recording

*/

//--------------------------------------------------------------------------------------------------

}	// namespace JC