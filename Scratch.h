#include "JC/Common.h"
#include "JC/Math.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Buffer {
};

struct Image {
};

struct Camera {
};

struct Scene {
	Camera camera;
	Vec3 ambient;
};

struct Mesh {
	s8 name;
	Buffer vertexBuffer;
	Buffer indexBuffer;
};

struct Texture {
	s8 name;
	Image image;
};

struct Pipeline {
	vertex shader;
	pixel shader;
};

struct Material {
	s8 name;
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