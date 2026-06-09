#pragma once

#include <SDL3/SDL_audio.h>
#include <string>
#include <vector>
#include <cstdint>

// Number of frequency/waveform bins exposed to effects and shaders.
// Matches AVS_Remake's BINS constant.
static constexpr int kAudioBins    = 576;
// FFT window size. 2048-point real FFT → 1024 frequency bins, resampled to kAudioBins.
static constexpr int kAudioFFTSize = 2048;

// Per-frame audio analysis result, layout matches the JS AudioAnalyzer.visdata array:
//   spec[channel][bin] : frequency spectrum, 0–255 scale
//   osc [channel][bin] : time-domain waveform, 0–255 scale (128 = silence)
//   channel 0 = left, 1 = right
struct VisData
{
    float spec[2][kAudioBins] = {};
    float osc [2][kAudioBins] = {};
};

struct AudioDeviceInfo
{
    SDL_AudioDeviceID Id;
    std::string       Name;
};

class AudioAnalyzer
{
public:
    bool Init();
    void Shutdown();

    // Enumerate available recording devices. Requires SDL audio subsystem to be live.
    static std::vector<AudioDeviceInfo> GetCaptureDevices();

    // Connect to a recording device (closes any current stream first).
    // Pass SDL_AUDIO_DEVICE_DEFAULT_RECORDING for the system default.
    bool ConnectDevice(SDL_AudioDeviceID deviceId);
    void Disconnect();

    bool IsConnected() const { return m_stream != nullptr || !m_filePcm.empty(); }
    bool IsFileMode()  const { return !m_filePcm.empty(); }

    // Decode an MP3 file and use it as the audio source (loops continuously).
    // Disconnects any live recording device first.
    bool ConnectFile(const std::string& path);

    // Call once per frame from Engine::Tick(), before uploading the audio texture.
    void Update();

    const VisData& GetVisData() const { return m_visdata; }
    bool           IsBeat()    const { return m_isBeat;   }

private:
    void UpdateFromDevice();
    void UpdateFromFile();
    void SlidePcm(const float* interleaved, int frames);
    void RunFFT();
    void DetectBeat();

    SDL_AudioStream* m_stream    = nullptr; // recording input stream
    SDL_AudioStream* m_outStream = nullptr; // playback output stream (file mode only)

    // MP3 file playback: decoded stereo PCM (interleaved L,R) and a looping cursor.
    std::vector<float> m_filePcm;            // empty when not in file mode
    int                m_filePos        = 0; // current frame index into m_filePcm
    int                m_fileSampleRate = 44100;

    // Sliding PCM buffer: always holds the most recent kAudioFFTSize samples per channel.
    float m_pcmL[kAudioFFTSize] = {};
    float m_pcmR[kAudioFFTSize] = {};

    // Temporary interleaved read buffer (kAudioFFTSize stereo frames).
    float m_readBuf[kAudioFFTSize * 2] = {};

    // Hann window coefficients.
    float m_window[kAudioFFTSize] = {};

    // kissfft config (opaque; stored as void* to keep this header free of kiss_fftr.h).
    void* m_fftCfg = nullptr;

    // Per-channel smoothed FFT magnitude output (kAudioFFTSize/2 + 1 bins).
    float m_smoothSpec[2][kAudioFFTSize / 2 + 1] = {};

    VisData m_visdata = {};
    bool    m_hasData = false;
    bool    m_isBeat  = false;

    // Beat detection state (matches JS AudioAnalyzer._detectBeat()).
    float m_beatPeak1     = 0.0f;
    float m_beatPeak2     = 0.0f;
    int   m_beatCnt       = 0;
    float m_beatPeak1Peak = 0.0f;
};
