#include "JC/Sprite.h"

#include "JC/Array.h"
#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Map.h"
#include "JC/Math.h"
#include "JC/Render.h"
#include "3rd/stb/stb_image.h"

namespace JC::Sprite {

//--------------------------------------------------------------------------------------------------

DefErr(Sprite, LoadImage);
DefErr(Sprite, ImageFmt);
DefErr(Sprite, AtlasFmt);
DefErr(Sprite, AlreadyExists);

//--------------------------------------------------------------------------------------------------

struct AtlasEntry {
	U32  imageIdx = 0;
	Str  name     = {};
	Vec2 uv1      = {};
	Vec2 uv2      = {};
};

static Mem::Allocator*      allocator;
static Array<Render::Image> atlasImages;
static Array<AtlasEntry>    atlasEntries;
static Map<Str, U32>        atlasEntryMap;

//--------------------------------------------------------------------------------------------------

void Init(Mem::Allocator* allocatorIn) {
	allocator = allocatorIn;
	atlasImages.Init(allocatorIn);
	atlasEntries.Init(allocatorIn);
	atlasEntryMap.Init(allocatorIn);
}

//--------------------------------------------------------------------------------------------------

Res<Render::Image> LoadImage(Str path) {
	Span<U8> data;
	if (Res<> r = File::ReadAll(Mem::tempAllocator, path).To(data); !r) { return r.err; }

	int width = 0;
	int height = 0;
	int channels = 0;
	U8* imageData = (U8*)stbi_load_from_memory(data.data, (int)data.len, &width, &height, &channels, 0);
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
		U8* in = imageData;
		U8* out = (U8*)stagingMem.ptr;
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

Res<> LoadAtlasImpl(Str path, Render::Image image, U32 imageIdx) {
	Span<U8> data;
	if (Res<> r = File::ReadAll(Mem::tempAllocator, path).To(data); !r) { return r.err; }	// TODO: ctx
	Json::Doc* doc = 0;
	if (Res<> r = Json::Parse(Mem::tempAllocator, Mem::tempAllocator, Str((const char*)data.data, data.len)).To(doc); !r) { return r.err; }	// TODO: ctx

	Json::Elem root = Json::GetRoot(doc);
	if (root.type != Json::Type::Arr) { return Err_AtlasFmt(); }	// TODO: ctx
	Span<Json::Elem> elems = Json::GetArr(doc, root);

	for (U64 i = 0; i < elems.len; i++) {
		if (elems[i].type != Json::Type::Obj) { return Err_AtlasFmt(); }	// TODO: ctx
		Json::Obj obj = Json::GetObj(doc, elems[i]);
		if (obj.keys.len != 5) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[0]) != "x"   ) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[0]) != "y"   ) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[0]) != "w"   ) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[0]) != "h"   ) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[0]) != "name") { return Err_AtlasFmt(); }

		const U64 ix   = Json::GetU64(doc, obj.vals[0]);
		const U64 iy   = Json::GetU64(doc, obj.vals[1]);
		const U64 iw   = Json::GetU64(doc, obj.vals[2]);
		const U64 ih   = Json::GetU64(doc, obj.vals[3]);
		const Str name = Json::GetStr(doc, obj.vals[4]);

		const F32 imageWidth = (F32)Render::GetImageWidth(image);
		const F32 imageHeight = (F32)Render::GetImageHeight(image);
		const F32 x = (F32)ix / imageWidth;
		const F32 y = (F32)iy / imageHeight;
		const F32 w = (F32)iw / imageWidth;
		const F32 h = (F32)ih / imageHeight;

		AtlasEntry* entry = atlasEntries.Add();
		entry->imageIdx = imageIdx;
		entry->uv1      = { x, y },
		entry->uv2      = { x + w, y + h },
		entry->name     = Copy(allocator, name);

		if (atlasEntryMap.FindOrNull(name)) {
			return Err_AlreadyExists("path", path, "name", name);
		}
		atlasEntryMap.Put(entry->name, (U32)(entry - atlasEntries.data));
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> LoadAtlas(Str imagePath, Str atlasPath) {
	Render::Image image;
	if (Res<> r = LoadImage(imagePath).To(image); !r) { return r; }
	const U32 imageIdx = Render::BindImage(image);
	return LoadAtlasImpl(atlasPath, image, imageIdx);
}

//--------------------------------------------------------------------------------------------------

Sprite Get(Str name) {
	name;
	return Sprite();
}

//--------------------------------------------------------------------------------------------------

Res<> Draw(Span<Sprite> sprites) {
	sprites;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Sprite