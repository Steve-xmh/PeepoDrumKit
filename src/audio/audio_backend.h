#pragma once
#include "core_types.h"
#include <functional>
#include <memory>

namespace Audio
{
	enum class StreamShareMode : u8
	{
		Shared,
		Exclusive,
		Count
	};

	struct BackendStreamParam
	{
		u32 SampleRate;
		u32 ChannelCount;
		u32 DesiredFrameCount;
		StreamShareMode ShareMode;
	};

	using BackendRenderCallback = std::function<void(i16 *outputBuffer, const u32 bufferFrameCount, const u32 bufferChannelCount)>;

	struct IAudioBackend
	{
		virtual ~IAudioBackend() = default;
		virtual b8 OpenStartStream(const BackendStreamParam &param, BackendRenderCallback callback) = 0;
		virtual b8 StopCloseStream() = 0;
		virtual b8 IsOpenRunning() const = 0;

		virtual u32 GetVariantCount() const = 0;
		virtual cstr GetVariantName(u32 index) const = 0;
	};

#ifdef _WIN32
	class WASAPIBackend : public IAudioBackend
	{
	public:
		WASAPIBackend();
		~WASAPIBackend();

	public:
		b8 OpenStartStream(const BackendStreamParam &param, BackendRenderCallback callback) override;
		b8 StopCloseStream() override;
		b8 IsOpenRunning() const override;

		u32 GetVariantCount() const override;
		cstr GetVariantName(u32 index) const override;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
	};
#endif // _WIN32

#ifdef __APPLE__
	class CoreAudioBackend : public IAudioBackend
	{
	public:
		CoreAudioBackend();
		~CoreAudioBackend();

	public:
		b8 OpenStartStream(const BackendStreamParam &param, BackendRenderCallback callback) override;
		b8 StopCloseStream() override;
		b8 IsOpenRunning() const override;

		u32 GetVariantCount() const override;
		cstr GetVariantName(u32 index) const override;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
	};
#endif // __APPLE__

	class LibSoundIOBackend : public IAudioBackend
	{
	public:
		LibSoundIOBackend();
		~LibSoundIOBackend();

	public:
		b8 OpenStartStream(const BackendStreamParam &param, BackendRenderCallback callback) override;
		b8 StopCloseStream() override;
		b8 IsOpenRunning() const override;

		u32 GetVariantCount() const override;
		cstr GetVariantName(u32 index) const override;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
	};

#ifdef __linux__
	class PipeWireBackend : public IAudioBackend
	{
	public:
		PipeWireBackend();
		~PipeWireBackend();

	public:
		b8 OpenStartStream(const BackendStreamParam &param, BackendRenderCallback callback) override;
		b8 StopCloseStream() override;
		b8 IsOpenRunning() const override;

		u32 GetVariantCount() const override;
		cstr GetVariantName(u32 index) const override;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
	};
#endif // __linux__
}
