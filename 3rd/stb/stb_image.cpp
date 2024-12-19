//#define STBI_ASSERT(x)
//#define STBI_MALLOC(size)
//#define STBI_REALLOC(ptr, newSize)
//#define STBI_FREE(ptr)

#define STBI_NO_STDIO

#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"



//STBIDEF stbi_uc *stbi_load_from_memory   (stbi_uc           const *buffer, int len   , int *x, int *y, int *channels_in_file, int desired_channels);
//STBIDEF stbi_uc *stbi_load_from_callbacks(stbi_io_callbacks const *clbk  , void *user, int *x, int *y, int *channels_in_file, int desired_channels);

//typedef struct
//{
//   int      (*read)  (void *user,char *data,int size);   // fill 'data' with 'size' bytes.  return number of bytes actually read
//   void     (*skip)  (void *user,int n);                 // skip the next 'n' bytes, or 'unget' the last -n bytes if negative
//   int      (*eof)   (void *user);                       // returns nonzero if we are at end of file/data
//} stbi_io_callbacks;

// The function stbi_failure_reason() can be queried for an extremely brief, end-user
// unfriendly explanation of why the load failed.