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
	private:
		static void soundio_impl_write_callback(struct SoundIoOutStream *outstream,
												int frame_count_min, int frame_count_max)
		{
			auto backend = static_cast<LibSoundIOBackend::Impl *>(outstream->userdata);
			if (backend == nullptr || !backend->isOpenRunning.load())
				return;

			struct SoundIoChannelArea *areas;
			int frames_left = Clamp((int)(outstream->software_latency * outstream->sample_rate), frame_count_min, frame_count_max);
			if (frame_count_min == 0)
				frames_left = (int)(outstream->sample_rate * 0.002); // 2ms buffer
			backend->renderBuffer.resize(static_cast<size_t>(frame_count_max * outstream->layout.channel_count));

			while (frames_left > 0)
			{
				int frame_count = frames_left;
				int err = 0;

				if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count)))
				{
					printf("Error beginning write to SoundIO outstream: %s\n", soundio_strerror(err));
					return;
				}

				if (!backend->renderCallback)
				{
					// No render callback; fill silence
					std::fill(backend->renderBuffer.begin(), backend->renderBuffer.end(), 0);
				}
				else
				{
					backend->renderCallback(backend->renderBuffer.data(), static_cast<u32>(frame_count), static_cast<u32>(outstream->layout.channel_count));
				}

				for (int frame = 0; frame < frame_count; ++frame)
				{
					for (int channel = 0; channel < outstream->layout.channel_count; ++channel)
					{
						i16 *ptr = reinterpret_cast<i16 *>(areas[channel].ptr);
						ptr[frame * areas[channel].step / sizeof(i16)] = backend->renderBuffer[static_cast<size_t>(frame * outstream->layout.channel_count + channel)];
					}
				}

				if ((err = soundio_outstream_end_write(outstream)))
				{
					printf("Error ending write to SoundIO outstream: %s\n", soundio_strerror(err));
					return;
				}

				soundio_outstream_get_latency(outstream, &backend->lastRenderLatency);
				// printf("SoundIO render latency: %.3f ms\n", backend->lastRenderLatency * 1000.0);

				frames_left -= frame_count;
			}
		}

		static void soundio_thread_entry_point(LibSoundIOBackend::Impl *impl)
		{
			if (impl == nullptr)
				return;

			printf("SoundIO render thread started.\n");

			while (!impl->renderThreadStopRequested.load())
				soundio_wait_events(impl->soundio);

			printf("SoundIO render thread exiting.\n");
		}

	public:
		b8 OpenStartStream(const BackendStreamParam &param, BackendRenderCallback callback)
		{
			std::lock_guard<std::mutex> lock(soundioMutex);
			if (isOpenRunning.load())
				return false;

			soundio = soundio_create();
			auto n_soundio_backend = soundio_backend_count(soundio);

			for (int i = 0; i < n_soundio_backend; ++i)
			{
				auto backend = soundio_get_backend(soundio, i);
				auto backend_name = soundio_backend_name(backend);
				printf("SoundIO Backend %d: %s\n", i, backend_name);
			}

			auto err = 0;

			if ((err = soundio_connect(soundio)))
			{
				printf("Error connecting to SoundIO: %s\n", soundio_strerror(err));
				soundio_destroy(soundio);
				soundio = nullptr;
				return false;
			}

			soundio_flush_events(soundio);

			auto def_device_index = soundio_default_output_device_index(soundio);

			if (def_device_index < 0)
			{
				printf("No default output device found.\n");
				soundio_destroy(soundio);
				soundio = nullptr;
				return false;
			}

			outputDevice = soundio_get_output_device(soundio, def_device_index);
			auto outputDeviceName = std::string(outputDevice->name);

			if (param.ShareMode == StreamShareMode::Exclusive && !outputDevice->is_raw)
			{
				// Try to find a raw device with the same name
				auto device_count = soundio_output_device_count(soundio);
				for (int i = 0; i < device_count; ++i)
				{
					auto device = soundio_get_output_device(soundio, i);
					if (device->is_raw && outputDeviceName == std::string(device->name))
					{
						soundio_device_unref(outputDevice);
						outputDevice = device;
						printf("Using raw output device for exclusive mode: %s\n", outputDevice->name);
						break;
					}
					soundio_device_unref(device);
				}
			}
			else
			{
				printf("Using default output device for shared mode: %s\n", outputDevice->name);
			}

			if (outputDevice == nullptr)
			{
				printf("Failed to get default output device.\n");
				soundio_destroy(soundio);
				soundio = nullptr;
				return false;
			}

			printf("Using output device: %s\n", outputDevice->name);

			struct SoundIoOutStream *outstream_local = soundio_outstream_create(outputDevice);

			if (outstream_local == nullptr)
			{
				printf("Failed to create SoundIO outstream.\n");
				soundio_device_unref(outputDevice);
				soundio_destroy(soundio);
				soundio = nullptr;
				outputDevice = nullptr;
				return false;
			}

			outstream_local->format = SoundIoFormatS16LE;
			outstream_local->userdata = static_cast<void *>(this);
			outstream_local->sample_rate = soundio_device_nearest_sample_rate(outputDevice, static_cast<int>(param.SampleRate));
			auto layout = soundio_channel_layout_get_default(param.ChannelCount);
			outstream_local->layout.name = layout->name;
			outstream_local->layout.channel_count = layout->channel_count;
			for (int i = 0; i < layout->channel_count; ++i)
			{
				outstream_local->layout.channels[i] = layout->channels[i];
			}
			outstream_local->write_callback = soundio_impl_write_callback;

			if ((err = soundio_outstream_open(outstream_local)))
			{
				printf("unable to open device: %s", soundio_strerror(err));
				soundio_outstream_destroy(outstream_local);
				soundio_device_unref(outputDevice);
				soundio_destroy(soundio);
				soundio = nullptr;
				outputDevice = nullptr;
				return false;
			}

			soundio_outstream_clear_buffer(outstream_local);

			if (outstream_local->layout_error)
			{
				printf("unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));
				soundio_outstream_destroy(outstream_local);
				soundio_device_unref(outputDevice);
				soundio_destroy(soundio);
				soundio = nullptr;
				outputDevice = nullptr;
				return false;
			}

			if ((err = soundio_outstream_start(outstream_local)))
			{
				printf("unable to start device: %s", soundio_strerror(err));
				return false;
			}

			printf("SoundIO outstream started: %d Hz, %d channels, %.3f ms latency\n", outstream_local->sample_rate, outstream_local->layout.channel_count, outstream_local->software_latency * 1000.0);
			// Store params and callback to member variables

			streamParam = param;
			renderCallback = std::move(callback);

			// Save the outstream to member so StopCloseStream can destroy it
			outstream = outstream_local;

			renderThread = std::thread(soundio_thread_entry_point, this);

			isOpenRunning.store(true);
			return true;
		}

		b8 StopCloseStream()
		{
			std::lock_guard<std::mutex> lock(soundioMutex);
			if (!isOpenRunning.load())
				return false;
			isOpenRunning.store(false);
			renderThreadStopRequested = true;
			soundio_wakeup(soundio);
			printf("Stopping SoundIO render thread...\n");
			renderThread.join();
			printf("SoundIO render thread stopped.\n");
			renderThreadStopRequested = false;
			soundio_outstream_destroy(outstream);
			soundio_device_unref(outputDevice);
			soundio_destroy(soundio);
			soundio = nullptr;
			outputDevice = nullptr;
			outstream = nullptr;

			return true;
		}

	public:
		b8 IsOpenRunning() const
		{
			return isOpenRunning.load();
		}

	private:
		BackendStreamParam streamParam = {};
		BackendRenderCallback renderCallback;

		std::mutex soundioMutex;
		SoundIo *soundio = nullptr;
		SoundIoDevice *outputDevice = nullptr;
		SoundIoOutStream *outstream = nullptr;

		std::thread renderThread;
		std::atomic<b8> isOpenRunning = false;
		std::atomic<b8> renderThreadStopRequested = false;
		std::vector<i16> renderBuffer;

		b8 applySharedSessionVolume = true;
		double lastRenderLatency = 0.0;
	};

	LibSoundIOBackend::LibSoundIOBackend() : impl(std::make_unique<Impl>()) {}
	LibSoundIOBackend::~LibSoundIOBackend() = default;
	b8 LibSoundIOBackend::OpenStartStream(const BackendStreamParam &param, BackendRenderCallback callback) { return impl->OpenStartStream(param, std::move(callback)); }
	b8 LibSoundIOBackend::StopCloseStream() { return impl->StopCloseStream(); }
	b8 LibSoundIOBackend::IsOpenRunning() const { return impl->IsOpenRunning(); }
}
