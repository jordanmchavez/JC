#include "JC/Sprite.h"
#include "JC/File.h"
#include "JC/Json.h"
#include "JC/Render.h"
#include "3rd/stb/stb_image.h"

namespace JC::Sprite {

//--------------------------------------------------------------------------------------------------

Res<Render::Image> LoadImage(Str path) {
	Span<u8> data;
	if (Res<> r = File::ReadAll(Mem::tempAllocator, path).To(data); !r) { return r.err; }

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

//--------------------------------------------------------------------------------------------------

Res<> LoadAtlas(Str path) {
	Span<u8> data;
	if (Res<> r = File::ReadAll(Mem::tempAllocator, path).To(data); !r) { return r.err; }
	return Json::Parse(, temp, s8((const char*)data.data, data.len));

	Json::Elem jsonRoot = {};
	if (Res<> r = LoadJson(Fmt(temp, "Assets/{}.atlas", fileName)).To(jsonRoot); !r) { return r; }

	Span<Json::Elem> jsonArr = {};
	if (Res<> r = Json::GetArr(jsonRoot).To(jsonArr); !r) { return r; }

	const f32 imageWidth  = (float)Render::GetImageWidth(image);
	const f32 imageHeight = (float)Render::GetImageHeight(image);
	for (u64 i = 0; i < jsonArr.len; i++) {
		Span<Json::KeyVal> jsonObj = {};
		if (Res<> r = Json::GetObj(jsonArr[i]).To(jsonObj); !r) { return r; }
		Assert(jsonObj[0].key == "x");
		Assert(jsonObj[1].key == "y");
		Assert(jsonObj[2].key == "w");
		Assert(jsonObj[3].key == "h");
		Assert(jsonObj[4].key == "name");

		i64 ix    = 0;
		i64 iy    = 0;
		i64 iw    = 0;
		i64 ih    = 0;
		s8  name = {};

		if (Res<> r = Json::GetI64(jsonObj[0].val).To(ix);   !r) { return r; }
		if (Res<> r = Json::GetI64(jsonObj[1].val).To(iy);   !r) { return r; }
		if (Res<> r = Json::GetI64(jsonObj[2].val).To(iw);   !r) { return r; }
		if (Res<> r = Json::GetI64(jsonObj[3].val).To(ih);   !r) { return r; }
		if (Res<> r = Json::GetS8 (jsonObj[4].val).To(name); !r) { return r; }

		const f32 x = (f32)ix / imageWidth;
		const f32 y = (f32)iy / imageHeight;
		const f32 w = (f32)iw / imageWidth;
		const f32 h = (f32)ih / imageHeight;

		AtlasEntry* entry = atlasEntries.Add();
		entry->imageIdx = imageIdx;
		entry->uv1      = { x, y },
		entry->uv2      = { x + w, y + h },
		entry->name     = Copy(perm, name);

		if (name != "") {
			if (atlasEntryMap.Find(name)) {
				return Err_AtlasNameAlreadyExists("filename", fileName, "name", name);
			}
			atlasEntryMap.Put(entry->name, (u32)(entry - atlasEntries.data));
		}
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Load(Str imagePath, Str atlasPath) {
	LoadImage(imagePath);
	LoadAtlas(atlasPath);
	const u32 imageIdx = Render::BindImage(image);
}

//--------------------------------------------------------------------------------------------------

Sprite Get(Str name) {
}

//--------------------------------------------------------------------------------------------------

Res<> Draw(Span<Sprite> sprites) {
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sprite