#include "audio_backend.h"
#include "audio_common.h"

#include <stdio.h>
#include <atomic>

#include <soundio/soundio.h>

// TODO: Implement LibSoundIO backend
namespace Audio
{
	struct LibSoundIOBackend::Impl
	{
	public:
		b8 OpenStartStream(const BackendStreamParam& param, BackendRenderCallback callback)
		{
			if (isOpenRunning)
				return false;

			isOpenRunning = true;
			return true;
		}

		b8 StopCloseStream()
		{
			if (!isOpenRunning)
				return false;
			isOpenRunning = false;

			return true;
		}

	public:
		b8 IsOpenRunning() const
		{
			return isOpenRunning;
		}

	public:

	private:
		BackendStreamParam streamParam = {};
		BackendRenderCallback renderCallback;

		std::atomic<b8> isOpenRunning = false;
		std::atomic<b8> renderThreadStopRequested = false;

		b8 applySharedSessionVolume = true;
	};

	LibSoundIOBackend::LibSoundIOBackend() : impl(std::make_unique<Impl>()) {}
	LibSoundIOBackend::~LibSoundIOBackend() = default;
	b8 LibSoundIOBackend::OpenStartStream(const BackendStreamParam& param, BackendRenderCallback callback) { return impl->OpenStartStream(param, std::move(callback)); }
	b8 LibSoundIOBackend::StopCloseStream() { return impl->StopCloseStream(); }
	b8 LibSoundIOBackend::IsOpenRunning() const { return impl->IsOpenRunning(); }
}
