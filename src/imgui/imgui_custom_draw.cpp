#include "imgui_custom_draw.h"

struct WAVEFORM_CONSTANT_BUFFER
{
	struct
	{
		vec2 Pos, UV;
	} PerVertex[6];
	struct
	{
		vec2 Size, SizeInv;
	} CB_RectSize;
	struct
	{
		f32 R, G, B, A;
	} Color;
	float Amplitudes[CustomDraw::WaveformPixelsPerChunk];
};

// TODO: Reimplement these on SDL3/SDL_GPU backend
namespace CustomDraw
{
	void GPUTexture::Load(const GPUTextureDesc &desc)
	{
	}

	void GPUTexture::Unload()
	{
	}

	void GPUTexture::UpdateDynamic(ivec2 size, const void *newPixels)
	{
	}

	b8 GPUTexture::IsValid() const { return 0; }
	ivec2 GPUTexture::GetSize() const { return ivec2(0, 0); }
	vec2 GPUTexture::GetSizeF32() const { return vec2(GetSize()); }
	GPUPixelFormat GPUTexture::GetFormat() const { return GPUPixelFormat {}; }
	ImTextureID GPUTexture::GetTexID() const { return 0; }

	void DrawWaveformChunk(ImDrawList *drawList, Rect rect, u32 color, const WaveformChunk &chunk)
	{
	}
}
