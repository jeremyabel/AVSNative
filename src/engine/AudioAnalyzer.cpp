#include "engine/AudioAnalyzer.h"

#include <kiss_fftr.h>
#include <SDL3/SDL.h>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_FLOAT_OUTPUT
#include <dr_mp3.h>

#include <algorithm>
#include <cmath>
#include <cstring>

static constexpr float kPi = 3.14159265358979323846f;

// ─── Init / Shutdown ─────────────────────────────────────────────────────────

bool AudioAnalyzer::Init()
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        SDL_Log("AudioAnalyzer: SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        return false;
    }

    // Pre-compute Hann window.
    for (int i = 0; i < kAudioFFTSize; i++)
        m_window[i] = 0.5f * (1.0f - cosf(2.0f * kPi * i / (kAudioFFTSize - 1)));

    // Allocate kissfft real-FFT plan.
    m_fftCfg = kiss_fftr_alloc(kAudioFFTSize, 0, nullptr, nullptr);
    if (!m_fftCfg)
    {
        SDL_Log("AudioAnalyzer: kiss_fftr_alloc failed");
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    return true;
}

void AudioAnalyzer::Shutdown()
{
    Disconnect();

    if (m_fftCfg)
    {
        kiss_fftr_free((kiss_fftr_cfg)m_fftCfg);
        m_fftCfg = nullptr;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

// ─── Device enumeration ───────────────────────────────────────────────────────

std::vector<AudioDeviceInfo> AudioAnalyzer::GetCaptureDevices()
{
    std::vector<AudioDeviceInfo> result;
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioRecordingDevices(&count);
    if (!ids) return result;

    for (int i = 0; i < count; i++)
    {
        AudioDeviceInfo info;
        info.Id   = ids[i];
        const char* name = SDL_GetAudioDeviceName(ids[i]);
        info.Name = name ? name : "(unknown)";
        result.push_back(std::move(info));
    }
    SDL_free(ids);
    return result;
}

// ─── Connect / Disconnect ─────────────────────────────────────────────────────

bool AudioAnalyzer::ConnectFile(const std::string& path)
{
    Disconnect(); // closes any live recording stream

    drmp3_config cfg{};
    drmp3_uint64 frameCount = 0;

    // Decode to stereo float at the native sample rate; dr_mp3 handles mono→stereo upmix.
    float* decoded = drmp3_open_file_and_read_pcm_frames_f32(
        path.c_str(), &cfg, &frameCount, nullptr);

    if (!decoded)
    {
        SDL_Log("AudioAnalyzer: failed to decode MP3: %s", path.c_str());
        return false;
    }

    // Store as stereo interleaved (L, R). drmp3 always gives cfg.channels channels;
    // if the file is mono, upmix to stereo manually.
    int outFrames = (int)frameCount;
    m_filePcm.resize(outFrames * 2);

    if (cfg.channels >= 2)
    {
        memcpy(m_filePcm.data(), decoded, outFrames * 2 * sizeof(float));
    }
    else
    {
        // Mono → stereo: duplicate the single channel.
        for (int i = 0; i < outFrames; i++)
        {
            m_filePcm[i * 2]     = decoded[i];
            m_filePcm[i * 2 + 1] = decoded[i];
        }
    }

    drmp3_free(decoded, nullptr);

    m_filePos        = 0;
    m_fileSampleRate = (int)cfg.sampleRate;
    memset(m_pcmL, 0, sizeof(m_pcmL));
    memset(m_pcmR, 0, sizeof(m_pcmR));
    memset(m_smoothSpec, 0, sizeof(m_smoothSpec));
    m_hasData = false;

    // Open a playback stream so the decoded audio is audible.
    SDL_AudioSpec outSpec;
    outSpec.format   = SDL_AUDIO_F32;
    outSpec.channels = 2;
    outSpec.freq     = m_fileSampleRate;

    m_outStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &outSpec, nullptr, nullptr);
    if (m_outStream)
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_outStream));
    else
        SDL_Log("AudioAnalyzer: could not open playback device: %s", SDL_GetError());

    SDL_Log("AudioAnalyzer: loaded MP3 — %d frames at %d Hz", outFrames, m_fileSampleRate);
    return true;
}

bool AudioAnalyzer::ConnectDevice(SDL_AudioDeviceID deviceId)
{
    Disconnect(); // also clears file mode via m_filePcm.clear()

    // Request F32 stereo at 44100 Hz; SDL3 converts on the fly if the device differs.
    SDL_AudioSpec spec;
    spec.format   = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq     = 44100;

    m_stream = SDL_OpenAudioDeviceStream(deviceId, &spec, nullptr, nullptr);
    if (!m_stream)
    {
        SDL_Log("AudioAnalyzer: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        return false;
    }

    // SDL_OpenAudioDeviceStream starts paused — resume it.
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_stream));

    // Clear the PCM history so we don't mix data from different devices.
    memset(m_pcmL, 0, sizeof(m_pcmL));
    memset(m_pcmR, 0, sizeof(m_pcmR));
    memset(m_smoothSpec, 0, sizeof(m_smoothSpec));
    m_hasData = false;
    return true;
}

void AudioAnalyzer::Disconnect()
{
    if (m_stream)
    {
        SDL_DestroyAudioStream(m_stream);
        m_stream = nullptr;
    }
    if (m_outStream)
    {
        SDL_DestroyAudioStream(m_outStream);
        m_outStream = nullptr;
    }
    m_filePcm.clear();
    m_filePos = 0;
}

// ─── Update (called every frame) ─────────────────────────────────────────────

void AudioAnalyzer::Update()
{
    if (!m_filePcm.empty())
    {
        UpdateFromFile();
    }
    else if (m_stream)
    {
        UpdateFromDevice();
    }
}

void AudioAnalyzer::UpdateFromDevice()
{
    int avail = SDL_GetAudioStreamAvailable(m_stream);
    if (avail <= 0) return;

    // Cap read at one FFT window worth of stereo frames to stay near real-time.
    int maxBytes = kAudioFFTSize * 2 * (int)sizeof(float);
    int readBytes = std::min(avail, maxBytes);
    // Align to a whole stereo frame.
    readBytes = (readBytes / (2 * (int)sizeof(float))) * (2 * (int)sizeof(float));
    if (readBytes <= 0) return;

    int got = SDL_GetAudioStreamData(m_stream, m_readBuf, readBytes);
    if (got <= 0) return;

    int frames = got / (2 * (int)sizeof(float));
    SlidePcm(m_readBuf, frames);

    m_hasData = true;
    RunFFT();
    DetectBeat();
}

void AudioAnalyzer::UpdateFromFile()
{
    const int totalFrames = (int)(m_filePcm.size() / 2);
    if (totalFrames == 0) return;

    // How many frames to feed this tick.
    // When a playback stream is open, pace by its buffer level so we don't drift.
    // Target: keep ~2 frames of lookahead queued to cover fps jitter without adding
    // perceptible latency.
    int toFeed;
    if (m_outStream)
    {
        int queuedFrames = SDL_GetAudioStreamAvailable(m_outStream)
                           / (2 * (int)sizeof(float));
        const int kTarget = (m_fileSampleRate / 60) * 2;
        toFeed = std::max(0, kTarget - queuedFrames);
        toFeed = std::min(toFeed, kAudioFFTSize); // stay within m_readBuf
    }
    else
    {
        toFeed = m_fileSampleRate / 60;
    }

    if (toFeed == 0)
    {
        // Output buffer is already full; re-run analysis on existing PCM so the
        // visualizer keeps updating even while we pause feeding.
        RunFFT();
        DetectBeat();
        return;
    }

    // Read from file into m_readBuf, looping seamlessly.
    int filled = 0;
    while (filled < toFeed)
    {
        int want = std::min(toFeed - filled, totalFrames - m_filePos);
        memcpy(m_readBuf + filled * 2,
               m_filePcm.data() + m_filePos * 2,
               want * 2 * sizeof(float));
        filled    += want;
        m_filePos += want;
        if (m_filePos >= totalFrames) m_filePos = 0;
    }

    if (m_outStream)
        SDL_PutAudioStreamData(m_outStream, m_readBuf,
                               toFeed * 2 * (int)sizeof(float));

    SlidePcm(m_readBuf, toFeed);
    m_hasData = true;
    RunFFT();
    DetectBeat();
}

void AudioAnalyzer::SlidePcm(const float* interleaved, int frames)
{
    if (frames < kAudioFFTSize)
    {
        int keep = kAudioFFTSize - frames;
        memmove(m_pcmL, m_pcmL + frames, keep * sizeof(float));
        memmove(m_pcmR, m_pcmR + frames, keep * sizeof(float));
        for (int i = 0; i < frames; i++)
        {
            m_pcmL[keep + i] = interleaved[i * 2];
            m_pcmR[keep + i] = interleaved[i * 2 + 1];
        }
    }
    else
    {
        // Got a full window or more — keep only the most recent kAudioFFTSize frames.
        int offset = frames - kAudioFFTSize;
        for (int i = 0; i < kAudioFFTSize; i++)
        {
            m_pcmL[i] = interleaved[(offset + i) * 2];
            m_pcmR[i] = interleaved[(offset + i) * 2 + 1];
        }
    }
}

// ─── FFT + spectrum / waveform computation ────────────────────────────────────

void AudioAnalyzer::RunFFT()
{
    kiss_fftr_cfg cfg = (kiss_fftr_cfg)m_fftCfg;

    float windowed[kAudioFFTSize];
    kiss_fft_cpx freqL[kAudioFFTSize / 2 + 1];
    kiss_fft_cpx freqR[kAudioFFTSize / 2 + 1];

    // Left channel FFT.
    for (int i = 0; i < kAudioFFTSize; i++)
        windowed[i] = m_pcmL[i] * m_window[i];
    kiss_fftr(cfg, windowed, freqL);

    // Right channel FFT.
    for (int i = 0; i < kAudioFFTSize; i++)
        windowed[i] = m_pcmR[i] * m_window[i];
    kiss_fftr(cfg, windowed, freqR);

    // Compute magnitude in dB (matches JS AnalyserNode smoothingTimeConstant=0.5).
    // JS mapping: getFloatFrequencyData gives dBFS, then v = max(0,(dB+100)/100)*255.
    const float invHalf = 1.0f / (kAudioFFTSize / 2);
    const int   bins    = kAudioFFTSize / 2 + 1;

    for (int i = 0; i < bins; i++)
    {
        auto toDb255 = [&](kiss_fft_cpx c) -> float {
            float mag = sqrtf(c.r * c.r + c.i * c.i) * invHalf;
            float db  = 20.0f * log10f(fmaxf(mag, 1e-10f));
            return fmaxf(0.0f, (db + 100.0f) / 100.0f) * 255.0f;
        };
        m_smoothSpec[0][i] = 0.5f * m_smoothSpec[0][i] + 0.5f * toDb255(freqL[i]);
        m_smoothSpec[1][i] = 0.5f * m_smoothSpec[1][i] + 0.5f * toDb255(freqR[i]);
    }

    // Resample from kAudioFFTSize/2 bins → kAudioBins (linear interp, matches JS _resample).
    const int   srcLen = kAudioFFTSize / 2;
    const float scale  = (float)srcLen / kAudioBins;
    for (int i = 0; i < kAudioBins; i++)
    {
        float r  = i * scale;
        int   lo = (int)r;
        int   hi = std::min(lo + 1, srcLen - 1);
        float t  = r - lo;
        m_visdata.spec[0][i] = m_smoothSpec[0][lo] * (1.0f - t) + m_smoothSpec[0][hi] * t;
        m_visdata.spec[1][i] = m_smoothSpec[1][lo] * (1.0f - t) + m_smoothSpec[1][hi] * t;
    }

    // Waveform: resample most recent kAudioFFTSize PCM frames → kAudioBins.
    // JS mapping: v = (pcm * 0.5 + 0.5) * 255, clamped; 128 = silence.
    const float wScale = (float)kAudioFFTSize / kAudioBins;
    for (int i = 0; i < kAudioBins; i++)
    {
        float r  = i * wScale;
        int   lo = (int)r;
        int   hi = std::min(lo + 1, kAudioFFTSize - 1);
        float t  = r - lo;
        float vL = m_pcmL[lo] * (1.0f - t) + m_pcmL[hi] * t;
        float vR = m_pcmR[lo] * (1.0f - t) + m_pcmR[hi] * t;
        m_visdata.osc[0][i] = fmaxf(0.0f, fminf(255.0f, (vL * 0.5f + 0.5f) * 255.0f));
        m_visdata.osc[1][i] = fmaxf(0.0f, fminf(255.0f, (vR * 0.5f + 0.5f) * 255.0f));
    }
}

// ─── Beat detection ───────────────────────────────────────────────────────────
// Matches JS AudioAnalyzer._detectBeat() (simple mode, no BPM tracker).
// Uses the first kAudioBins samples of the raw PCM as the waveform energy source.

void AudioAnalyzer::DetectBeat()
{
    float ltL = 0.0f, ltR = 0.0f;
    for (int i = 0; i < kAudioBins; i++)
    {
        ltL += fabsf(m_pcmL[i]);
        ltR += fabsf(m_pcmR[i]);
    }
    float lt = ltL > ltR ? ltL : ltR;

    m_beatPeak1 = (m_beatPeak1 * 125.0f + m_beatPeak2 * 3.0f) / 128.0f;
    m_beatCnt++;

    // float equivalent of the original 576*16 integer threshold (÷128 for float amp 0..1).
    constexpr float kMinBeatEnergy = kAudioBins * 16.0f / 128.0f; // 72.0

    m_isBeat = false;
    if (lt >= (m_beatPeak1 * 34.0f) / 32.0f && lt > kMinBeatEnergy)
    {
        if (m_beatCnt > 0)
        {
            m_beatCnt = 0;
            m_isBeat  = true;
        }
        m_beatPeak1     = (lt + m_beatPeak1Peak) / 2.0f;
        m_beatPeak1Peak = lt;
    }
    else if (lt > m_beatPeak2)
    {
        m_beatPeak2 = lt;
    }
    else
    {
        m_beatPeak2 = (m_beatPeak2 * 14.0f) / 16.0f;
    }
}
