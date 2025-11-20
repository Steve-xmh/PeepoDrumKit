#include "imgui_application_host.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include "extension/imgui_input_binding.h"

#include "core_io.h"
#include "core_string.h"
#include "../src_res/resource.h"

#define HAS_EMBEDDED_ICONS 1
#define REGENERATE_EMBEDDED_ICONS_SOURCE_CODE 0

#if HAS_EMBEDDED_ICONS || REGENERATE_EMBEDDED_ICONS_SOURCE_CODE
#define STBI_WINDOWS_UTF8
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize2.h>
#endif

struct WindowRectWithDecoration
{
	ivec2 Position, Size;
};
struct ClientRect
{
	ivec2 Position, Size;
};

EmbeddedIconsList GetEmbeddedIconsList()
{
	return EmbeddedIconsList{0};
}

#ifdef _WIN32

#include <windows.h>
#include <bcrypt.h>

extern "C" NTSTATUS NTAPI NtSetTimerResolution(
	IN ULONG DesiredResolution,
	IN BOOLEAN SetResolution,
	OUT PULONG CurrentResolution);

extern "C" NTSTATUS NTAPI NtQueryTimerResolution(
	OUT PULONG CurrentResolution,
	OUT PULONG MinimumResolution,
	OUT PULONG MaximumResolution);

static void EnableWindowsHiResTimer(void)
{
	ULONG curRes = 0;
	ULONG minRes = 0;
	ULONG maxRes = 0;

	NTSTATUS queryStatus = NtQueryTimerResolution(&curRes, &minRes, &maxRes);
	if (queryStatus == 0)
	{
		bool setHighResolutionTimer = curRes > maxRes;

		if (setHighResolutionTimer)
		{
			ULONG targetResolution = curRes;
			NTSTATUS status = NtSetTimerResolution(maxRes, TRUE, &targetResolution);
			if (status == 0)
			{
				std::cout << "Using HiRes Timer: " << curRes / 10000.0 << "ms -> " << targetResolution / 10000.0 << "ms" << std::endl;
			}
		}
	}
}

#endif // _WIN32

namespace ApplicationHost
{
	// HACK: Color seen briefly during startup before the first frame has been drawn.
	//		 Should match the primary ImGui style window background color to create a seamless transition.
	//		 Hardcoded for now but should probably be passed in as a parameter / updated dynamically as the ImGui style changes (?)
	static constexpr u32 Win32WindowBackgroundColor = 0x001F1F1F;
	static constexpr cstr FontFilePath = "assets/NotoSansCJKjp-Regular.otf";

	static constexpr ImVec4 clearColor = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
	static ImFont *currentFont = nullptr;

	static void LoadFont(void)
	{
		auto &io = ImGui::GetIO();
		if (currentFont != nullptr)
		{
			io.Fonts->RemoveFont(currentFont);
			currentFont = nullptr;
		}

		std::string fontFilePath = "assets/" + FontMainFileNameCurrent;

		currentFont = io.Fonts->AddFontFromFileTTF(fontFilePath.c_str(), 16.0f);
		if (currentFont == nullptr)
		{
			std::cout << "Failed to load font file at: " << fontFilePath << std::endl;
		}
		else
		{
			std::cout << "Loaded font file at: " << fontFilePath << std::endl;
		}
	}

	static void BeforeRender(void)
	{
		if (FontMainFileNameTarget != FontMainFileNameCurrent)
		{
			FontMainFileNameCurrent = FontMainFileNameTarget;
			LoadFont();
		}
	}

	i32 EnterProgramLoop(const StartupParam &startupParam, UserCallbacks userCallbacks)
	{
#ifdef _WIN32
		EnableWindowsHiResTimer();
#endif // _WIN32

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
		{
			std::cout << "Error: SDL_Init(): " << SDL_GetError() << std::endl;
			return -1;
		}

		float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

		auto windowTitle = std::string(startupParam.WindowTitle.empty() ? "Peepo Drum Kit" : startupParam.WindowTitle);
		auto windowPos = startupParam.WindowPosition.has_value() ? *startupParam.WindowPosition : ivec2(100, 100);
		auto windowSize = startupParam.WindowSize.has_value() ? *startupParam.WindowSize : ivec2(1280, 720);

		SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
		auto window = SDL_CreateWindow(windowTitle.c_str(), (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
		if (window == nullptr)
		{
			std::cout << "Error: SDL_CreateWindow(): " << SDL_GetError() << std::endl;
			return -1;
		}

		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		SDL_ShowWindow(window);

		auto gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr);
		if (gpu_device == nullptr)
		{
			std::cout << "Error: SDL_CreateGPUDevice(): " << SDL_GetError() << std::endl;
			return 1;
		}

		if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
		{
			std::cout << "Error: SDL_ClaimWindowForGPUDevice(): " << SDL_GetError() << std::endl;
			return 1;
		}
		SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

		IMGUI_CHECKVERSION();
		auto ctx = ImGui::CreateContext();
		(void)ctx;
		// ctx->DebugLogFlags |= ImGuiDebugLogFlags_::ImGuiDebugLogFlags_EventDocking;
		auto &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	  // Enable Docking
		io.IniFilename = "settings_imgui.ini";
		// auto font = io.Fonts->AddFontFromFileTTF(FontFilePath, 16.0f);

		ImGui::StyleColorsDark();

		auto &style = ImGui::GetStyle();
		style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
		style.FontScaleDpi = main_scale;
		io.ConfigDpiScaleFonts = true;
		io.ConfigDpiScaleViewports = true;

		// Setup Platform/Renderer backends
		ImGui_ImplSDL3_InitForSDLGPU(window);
		ImGui_ImplSDLGPU3_InitInfo init_info = {};
		init_info.Device = gpu_device;
		init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
		init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1; // Only used in multi-viewports mode.
		ImGui_ImplSDLGPU3_Init(&init_info);

		userCallbacks.OnStartup();

		bool done = false;

		while (!done)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL3_ProcessEvent(&event);
				if (event.type == SDL_EVENT_QUIT)
					done = true;
				if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window) && userCallbacks.OnWindowCloseRequest() == CloseResponse::Exit)
					done = true;
			}

			if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
			{
				SDL_Delay(10);
				continue;
			}

			ImGui_ImplSDLGPU3_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();

			// Render here
			{
				BeforeRender();
				if (currentFont != nullptr)
					ImGui::PushFont(currentFont);
				userCallbacks.OnUpdate();
				if (currentFont != nullptr)
					ImGui::PopFont();
			}

			// Rendering
			ImGui::Render();
			auto draw_data = ImGui::GetDrawData();
			const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

			SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device); // Acquire a GPU command buffer

			SDL_GPUTexture *swapchain_texture;
			SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr); // Acquire a swapchain texture

			if (swapchain_texture != nullptr && !is_minimized)
			{
				// This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index buffer!
				ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

				// Setup and start a render pass
				SDL_GPUColorTargetInfo target_info = {};
				target_info.texture = swapchain_texture;
				target_info.clear_color = SDL_FColor{clearColor.x, clearColor.y, clearColor.z, clearColor.w};
				target_info.load_op = SDL_GPU_LOADOP_CLEAR;
				target_info.store_op = SDL_GPU_STOREOP_STORE;
				target_info.mip_level = 0;
				target_info.layer_or_depth_plane = 0;
				target_info.cycle = false;
				SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

				// Render ImGui
				ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

				SDL_EndGPURenderPass(render_pass);
			}

			// Submit the command buffer
			SDL_SubmitGPUCommandBuffer(command_buffer);
		}

		userCallbacks.OnShutdown();

		SDL_WaitForGPUIdle(gpu_device);
		ImGui_ImplSDL3_Shutdown();
		ImGui_ImplSDLGPU3_Shutdown();
		ImGui::DestroyContext();

		SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
		SDL_DestroyGPUDevice(gpu_device);
		SDL_DestroyWindow(window);
		SDL_Quit();

		return 0;
	}
}
