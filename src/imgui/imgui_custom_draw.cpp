#include "imgui_custom_draw.h"
#include "imgui_application_host.h"

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
		if (ApplicationHost::GlobalState.SDL_GPUDeviceHandle == nullptr)
		{
			IM_ASSERT(false && "SDL_GPUDeviceHandle is null!");
			return;
		}

		auto image_width = static_cast<u32>(desc.Size.x);
		auto image_height = static_cast<u32>(desc.Size.y);
		auto device = static_cast<SDL_GPUDevice *>(ApplicationHost::GlobalState.SDL_GPUDeviceHandle);
		// Create texture
		SDL_GPUTextureCreateInfo texture_info = {};
		texture_info.type = SDL_GPU_TEXTURETYPE_2D;
		if (desc.Format == GPUPixelFormat::RGBA)
			texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		else if (desc.Format == GPUPixelFormat::BGRA)
			texture_info.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
		else
			IM_ASSERT(false && "Unsupported GPUPixelFormat in GPUTexture::Load");
		texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
		texture_info.width = image_width;
		texture_info.height = image_height; 
		texture_info.layer_count_or_depth = 1;
		texture_info.num_levels = 1;
		texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
		
		auto sampler_info = SDL_GPUSamplerCreateInfo {
            .min_filter = SDL_GPU_FILTER_NEAREST,
            .mag_filter = SDL_GPU_FILTER_NEAREST,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		};

		this->Binding.texture = SDL_CreateGPUTexture(device, &texture_info);
		this->Binding.sampler = SDL_CreateGPUSampler(device, &sampler_info);

		// Create transfer buffer
		// FIXME: A real engine would likely keep one around, see what the SDL_GPU backend is doing.
		SDL_GPUTransferBufferCreateInfo transferbuffer_info = {};
		transferbuffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		transferbuffer_info.size = image_width * image_height * 4;
		SDL_GPUTransferBuffer *transferbuffer = SDL_CreateGPUTransferBuffer(device, &transferbuffer_info);
		IM_ASSERT(transferbuffer != NULL);

		// Copy to transfer buffer
		uint32_t upload_pitch = image_width * 4;
		void *texture_ptr = SDL_MapGPUTransferBuffer(device, transferbuffer, true);
		for (int y = 0; y < image_height; y++)
			memcpy((void *)((uintptr_t)texture_ptr + y * upload_pitch), (u8 *)desc.InitialPixels + y * upload_pitch, upload_pitch);
		SDL_UnmapGPUTransferBuffer(device, transferbuffer);

		SDL_GPUTextureTransferInfo transfer_info = {};
		transfer_info.offset = 0;
		transfer_info.transfer_buffer = transferbuffer;

		SDL_GPUTextureRegion texture_region = {};
		texture_region.texture = this->Binding.texture;
		texture_region.x = 0;
		texture_region.y = 0;
		texture_region.w = image_width;
		texture_region.h = image_height;
		texture_region.d = 1;

		// Upload
		SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
		SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
		SDL_UploadToGPUTexture(copy_pass, &transfer_info, &texture_region, false);
		SDL_EndGPUCopyPass(copy_pass);
		SDL_SubmitGPUCommandBuffer(cmd);

		SDL_ReleaseGPUTransferBuffer(device, transferbuffer);

		this->Size = desc.Size;
		this->Format = desc.Format;
		this->Access = desc.Access;
	}

	void GPUTexture::Unload()
	{
		auto device = static_cast<SDL_GPUDevice *>(ApplicationHost::GlobalState.SDL_GPUDeviceHandle);
		SDL_ReleaseGPUTexture(device, this->Binding.texture);
		SDL_ReleaseGPUSampler(device, this->Binding.sampler);
		this->Binding.texture = nullptr;
		this->Binding.sampler = nullptr;
		this->Access = GPUAccessType::Static;
		this->Size = ivec2(0, 0);
		this->Format = GPUPixelFormat::RGBA;
	}

	void GPUTexture::UpdateDynamic(ivec2 size, const void *newPixels)
	{
		// TODO: Reimplement since this is not in used by current code
	}

	b8 GPUTexture::IsValid() const { return this->Binding.texture != nullptr && this->Binding.sampler != nullptr; }
	ivec2 GPUTexture::GetSize() const { return this->Size; }
	vec2 GPUTexture::GetSizeF32() const { return vec2(GetSize()); }
	GPUPixelFormat GPUTexture::GetFormat() const { return this->Format; }
	ImTextureID GPUTexture::GetTexID() const { return (ImTextureID)(intptr_t)&this->Binding; }
	void DrawWaveformChunk(ImDrawList *drawList, Rect rect, u32 color, const WaveformChunk &chunk)
	{
		// TODO: Optimize with a single triangle strip or a custom GPU shader
		f32 lineThinkness = rect.GetWidth() / static_cast<f32>(WaveformPixelsPerChunk);
		for (i32 i = 0; i < WaveformPixelsPerChunk; i++)
		{
			f32 amplitude = chunk.PerPixelAmplitude[i];
			f32 x = rect.TL.x + (i * lineThinkness);
			f32 yCenter = rect.GetCenter().y;
			f32 yOffset = (amplitude * 0.5f) * rect.GetHeight();
			drawList->AddLine(ImVec2(x, yCenter - yOffset), ImVec2(x, yCenter + yOffset), color, lineThinkness);
		}
	}
}
