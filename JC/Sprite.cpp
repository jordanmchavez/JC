#include "JC/Sprite.h"

#include "JC/File.h"
#include "JC/Render.h"

namespace JC::Sprite {

//--------------------------------------------------------------------------------------------------

Res<Render::Image> LoadImage(Str path) {
	Span<u8> data;
	if (Res<> r = File::ReadAll(temp, path).To(data); !r) { return r.err; }

	int width = 0;
	int height = 0;
	int channels = 0;
	u8* imageData = (u8*)stbi_load_from_memory(data.data, (int)data.len, &width, &height, &channels, 0);
	if (!imageData) {
		return Err_LoadImage("path", path, "desc", stbi_failure_reason());
	}
	if (channels != 3 && channels != 4) {
		return Err_ImageFmt("path", path, "channels", channels);
	}
	Defer { stbi_image_free(imageData); };

	Render::Image image;
	if (Res<> r = Render::CreateImage(width, height, Render::ImageFormat::R8G8B8A8_UNorm, Render::ImageUsage::Sampled).To(image); !r) { return r.err; }

	Render::ImageBarrier(image, Render::Stage::None, Render::Stage::TransferDst);
	const Render::StagingMem stagingMem = Render::AllocStagingMem(width * height * 4);
	if (channels == 4) {
		memcpy(stagingMem.ptr, imageData, width * height * channels);
	} else {
		u8* in = imageData;
		u8* out = (u8*)stagingMem.ptr;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				*out++ = *in++;
				*out++ = *in++;
				*out++ = *in++;
				*out++ = 0xff;
			}
		}
	}
	Render::UpdateImage(image, stagingMem);
	Render::ImageBarrier(image, Render::Stage::TransferDst, Render::Stage::FragmentShaderSample);

	return image;
}

Res<> Load(Str imagePath, Str atlasPath) {
}

Sprite Get(Str name) {
}

Res<> Draw(Span<Sprite> sprites) {
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sprite