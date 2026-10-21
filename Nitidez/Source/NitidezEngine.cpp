#include "NitidezEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C"
{
#include "rnnoise.h"
}

namespace nitidez
{

static constexpr double kPi = 3.14159265358979323846;
static constexpr int kMaxChunk = 512;
static constexpr size_t kFifoCapacity = 16384;
static constexpr float kHoldFrames = 10.0f; // ~100 ms de "segurar" depois que a voz some

//==============================================================================
void Biquad::setLowpass (double sampleRate, double cutoffHz, double q)
{
    cutoffHz = std::min (cutoffHz, sampleRate * 0.49);
    const double w0 = 2.0 * kPi * cutoffHz / sampleRate;
    const double cs = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    b0 = ((1.0 - cs) * 0.5) / a0;
    b1 = (1.0 - cs) / a0;
    b2 = ((1.0 - cs) * 0.5) / a0;
    a1 = (-2.0 * cs) / a0;
    a2 = (1.0 - alpha) / a0;
    reset();
}

//==============================================================================
void CubicResampler::prepare (double inputRate, double outputRate, size_t reserve)
{
    inRate = std::llround (inputRate);
    outRate = std::llround (outputRate);
    buf.reserve (reserve);
    reset();
}

void CubicResampler::reset()
{
    buf.assign (1, 0.0f); // uma amostra de historico (zero)
    pos = outRate;        // 1.0 amostra
}

void CubicResampler::process (const float* in, int numIn, std::vector<float>& out)
{
    buf.insert (buf.end(), in, in + numIn);

    while (true)
    {
        const long long i = pos / outRate;
        if ((size_t) (i + 2) >= buf.size())
            break;

        const float t = (float) ((double) (pos % outRate) / (double) outRate);
        const float p0 = buf[(size_t) i - 1], p1 = buf[(size_t) i];
        const float p2 = buf[(size_t) i + 1], p3 = buf[(size_t) i + 2];
        const float y = p1 + 0.5f * t * (p2 - p0 + t * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3
                                                        + t * (3.0f * (p1 - p2) + p3 - p0)));
        out.push_back (y);
        pos += inRate;
    }

    const long long drop = pos / outRate - 1;
    if (drop > 0)
    {
        buf.erase (buf.begin(), buf.begin() + (long) drop);
        pos -= drop * outRate;
    }
}

//==============================================================================
void Fifo::prepare (size_t capacityPow2)
{
    data.assign (capacityPow2, 0.0f);
    mask = capacityPow2 - 1;
    readPos = writePos = 0;
}

//==============================================================================
Engine::~Engine()
{
    for (auto& ch : channels_)
        if (ch.rnn != nullptr)
            rnnoise_destroy (ch.rnn);
}

void Engine::prepare (double sampleRate, int numChannels)
{
    hostRate = sampleRate;
    needsResample = std::abs (sampleRate - 48000.0) > 0.5;
    numCh = std::max (1, std::min (numChannels, (int) kMaxChannels));

    const double r = sampleRate / 48000.0;
    const int algoDelayHost = (int) std::lround (kFrame * r);          // atraso proprio do RNNoise
    prefill = (int) std::ceil (kFrame * r) + (needsResample ? 6 : 0); // folga p/ quantizacao em quadros
    latencySamples = prefill + algoDelayHost;
    dryDelay = latencySamples;

    chunkBuf.assign ((size_t) kMaxChunk, 0.0f);

    for (int c = 0; c < kMaxChannels; ++c)
    {
        auto& ch = channels_[c];

        if (ch.rnn != nullptr)
            rnnoise_destroy (ch.rnn);
        ch.rnn = rnnoise_create (nullptr);

        ch.up.prepare (sampleRate, 48000.0, 16384);
        ch.down.prepare (48000.0, sampleRate, 16384);

        // Filtros anti-aliasing (Butterworth de 4a ordem) so quando necessario.
        ch.aaUp[0].setLowpass (sampleRate, 21000.0, 0.5411961);
        ch.aaUp[1].setLowpass (sampleRate, 21000.0, 1.3065630);
        ch.aaDown[0].setLowpass (48000.0, sampleRate * 0.46, 0.5411961);
        ch.aaDown[1].setLowpass (48000.0, sampleRate * 0.46, 1.3065630);

        ch.in48.clear();
        ch.in48.reserve (4096);
        ch.hostOut.clear();
        ch.hostOut.reserve (4096);
        ch.wet.prepare (kFifoCapacity);
        ch.dry.assign ((size_t) dryDelay, 0.0f);
    }

    reset();
}

void Engine::reset()
{
    for (auto& ch : channels_)
    {
        if (ch.rnn != nullptr)
        {
            rnnoise_destroy (ch.rnn);
            ch.rnn = rnnoise_create (nullptr);
        }

        ch.up.reset();
        ch.down.reset();
        for (auto& b : ch.aaUp) b.reset();
        for (auto& b : ch.aaDown) b.reset();

        ch.in48.clear();
        ch.hostOut.clear();
        ch.wet.clear();
        for (int i = 0; i < prefill; ++i)
            ch.wet.push (0.0f);

        std::fill (ch.dry.begin(), ch.dry.end(), 0.0f);
        ch.dryPos = 0;
        ch.gateGain = 1.0f;
        ch.gateHold = 0;
    }

    meterVoice = 0.0f;
    meterReduction = 0.0f;
    smoothedIn = inGain;
    smoothedOut = outGain;
    smoothedMix = mixAmount;
}

void Engine::setParameters (float clarity, float hush, float inputDb, float outputDb)
{
    clarity = std::min (1.0f, std::max (0.0f, clarity));
    // Curva: o ponteiro do meio ja remove bastante ruido (a reducao e percebida em dB, nao linear).
    mixAmount = 1.0f - (1.0f - clarity) * (1.0f - clarity);

    hushDb = std::min (30.0f, std::max (0.0f, hush));
    hushFloor = std::pow (10.0f, -hushDb / 20.0f);

    inGain = std::pow (10.0f, inputDb / 20.0f);
    outGain = std::pow (10.0f, outputDb / 20.0f);
}

//==============================================================================
void Engine::process (float* const* channels, int numChannels, int numSamples)
{
    const int n = std::min (numChannels, numCh);

    for (int offset = 0; offset < numSamples; offset += kMaxChunk)
    {
        const int len = std::min (kMaxChunk, numSamples - offset);

        for (int c = 0; c < n; ++c)
            processChunk (channels_[c], channels[c] + offset, len, c == 0);

        smoothedIn = inGain;
        smoothedOut = outGain;
        smoothedMix = mixAmount;
    }
}

void Engine::processChunk (Channel& ch, float* data, int len, bool /*first*/)
{
    // 1) ganho de entrada (com rampa para evitar cliques)
    const float in0 = smoothedIn, in1 = inGain;
    for (int i = 0; i < len; ++i)
    {
        const float g = in0 + (in1 - in0) * (float) (i + 1) / (float) len;
        chunkBuf[(size_t) i] = data[i] * g;
    }

    // 2) caminho processado: ate 48 kHz, quadros do RNNoise
    if (! needsResample)
    {
        ch.in48.insert (ch.in48.end(), chunkBuf.begin(), chunkBuf.begin() + len);
    }
    else
    {
        for (int i = 0; i < len; ++i)
        {
            float x = chunkBuf[(size_t) i];
            if (hostRate > 48000.0)
                x = ch.aaUp[1].process (ch.aaUp[0].process (x));
            data[i] = x; // data e reescrito no passo 3, entao serve de temporario
        }
        ch.up.process (data, len, ch.in48);
    }

    while ((int) ch.in48.size() >= kFrame)
        processFrame (ch);

    // 3) mistura com o original (alinhado pelo atraso) e ganho de saida
    const float m0 = smoothedMix, m1 = mixAmount;
    const float o0 = smoothedOut, o1 = outGain;
    const size_t ld = ch.dry.size();

    for (int i = 0; i < len; ++i)
    {
        const float t = (float) (i + 1) / (float) len;
        const float m = m0 + (m1 - m0) * t;
        const float og = o0 + (o1 - o0) * t;

        const float wet = ch.wet.available() > 0 ? ch.wet.pop() : 0.0f;

        const float dryNow = chunkBuf[(size_t) i];
        const float dryDelayed = ch.dry[ch.dryPos];
        ch.dry[ch.dryPos] = dryNow;
        if (++ch.dryPos >= ld)
            ch.dryPos = 0;

        data[i] = (dryDelayed * (1.0f - m) + wet * m) * og;
    }
}

void Engine::processFrame (Channel& ch)
{
    constexpr float kScale = 32768.0f; // RNNoise espera amostras na escala de 16 bits

    float eIn = 0.0f;
    for (int i = 0; i < kFrame; ++i)
    {
        scratch48[i] = ch.in48[(size_t) i] * kScale;
        eIn += scratch48[i] * scratch48[i];
    }
    ch.in48.erase (ch.in48.begin(), ch.in48.begin() + kFrame);

    const float vad = rnnoise_process_frame (ch.rnn, scratchOut, scratch48);

    // Hush: reduz o que sobra entre as frases, usando a probabilidade de voz do RNNoise.
    float target = 1.0f;
    if (hushFloor < 0.999f)
    {
        if (vad > 0.5f)
            ch.gateHold = (int) kHoldFrames;
        else if (ch.gateHold > 0)
            --ch.gateHold;
        target = ch.gateHold > 0 ? 1.0f : hushFloor;
    }
    const float coef = target > ch.gateGain ? 0.6f : 0.12f; // abre rapido, fecha devagar
    const float newGain = ch.gateGain + (target - ch.gateGain) * coef;

    float eOut = 0.0f;
    for (int i = 0; i < kFrame; ++i)
    {
        const float g = ch.gateGain + (newGain - ch.gateGain) * (float) (i + 1) / (float) kFrame;
        const float y = scratchOut[i] * g;
        eOut += y * y;
        scratch48[i] = y / kScale; // reaproveita o buffer: agora guarda a saida em escala normal
    }
    ch.gateGain = newGain;

    // Medidores
    meterVoice = std::max (vad, meterVoice * 0.92f);
    const float red = std::min (40.0f, std::max (0.0f, 10.0f * std::log10 ((eIn + 1.0f) / (eOut + 1.0f))));
    meterReduction += (red - meterReduction) * 0.2f;

    // 3) volta para a taxa do host
    if (! needsResample)
    {
        for (int i = 0; i < kFrame; ++i)
            ch.wet.push (scratch48[i]);
    }
    else
    {
        if (hostRate < 48000.0)
            for (int i = 0; i < kFrame; ++i)
                scratch48[i] = ch.aaDown[1].process (ch.aaDown[0].process (scratch48[i]));

        ch.hostOut.clear();
        ch.down.process (scratch48, kFrame, ch.hostOut);
        for (float v : ch.hostOut)
            ch.wet.push (v);
    }
}

} // namespace nitidez
