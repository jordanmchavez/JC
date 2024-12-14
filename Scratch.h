#include "JC/Common.h"
#include "JC/Math.h"

namespace JC {

//--------------------------------------------------------------------------------------------------


create a vb/ib for the various meshes in the scene in dedicated buffers each
load all textures
load shaders
create pipelines
setup all materials
now entities can be created dynamically: we just have a static array for max entities

struct Buffer {};
struct Texture {};
struct Pipeline {};
struct Shader {};

enum struct BufferUsage {
	Static,
	Staging,
};

enum struct Format {
	// ...
};

Shader CreateShader(void* data, u64 len);
Buffer CreateBuffer(u64 size, BufferUsage usage);
Texture CreateTexture(u32 width, u32 height, Format format);

struct Scene {
	Vec3 cameraPos;
	Vec3 cameraDir;
	Vec3 ambient;
};

struct Mesh {
	Buffer vertexBuffer;
	Buffer indexBuffer;
};

struct Pipeline {
	vertex shader;
	pixel shader;
	fill mode
	front face
	line width
	multisampling
	depth state / blending
};

struct Material {
	Texture diffuse;
	Vec3 color;
	Pipeline pipeline;
};

struct Entity {
	Mesh mesh;
	Material material;
	float position;
	Vec3 rotation;
};
//--------------------------------------------------------------------------------------------------

}	// namespace JC