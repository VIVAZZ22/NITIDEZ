#pragma once

// Motor de processamento do Nitidez (independente do JUCE, para poder ser testado sozinho).
//
// Fluxo por canal:
//   entrada -> ganho -> [reamostra p/ 48 kHz] -> RNNoise (quadros de 10 ms) -> Hush (gate suave)
//   -> [reamostra p/ taxa do host] -> mistura com o sinal original (Clarity) -> ganho de saida
//
// Nenhuma alocacao de memoria acontece em process(); tudo e reservado em prepare().

#include <cstddef>
#include <vector>

struct DenoiseState;

namespace nitidez
{

class Biquad
{
public:
    void setLowpass (double sampleRate, double cutoffHz, double q);
    void reset() { z1 = z2 = 0.0; }
    float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
};

// Reamostrador continuo (cubico, Catmull-Rom). A fase e mantida em aritmetica inteira (em unidades de
// 1/outputRate), entao o resultado e exatamente o mesmo, qualquer que seja o tamanho dos blocos do host.
class CubicResampler
{
public:
    void prepare (double inputRate, double outputRate, size_t reserve);
    void reset();
    void process (const float* in, int numIn, std::vector<float>& out);

private:
    long long inRate = 48000, outRate = 48000;
    long long pos = 48000; // posicao atual em unidades de 1/outRate (comeca em 1.0 amostra)
    std::vector<float> buf;
};

// Fila circular simples de floats.
class Fifo
{
public:
    void prepare (size_t capacityPow2);
    void clear() { readPos = writePos = 0; }
    size_t available() const { return writePos - readPos; }
    void push (float v) { data[writePos++ & mask] = v; }
    float pop() { return data[readPos++ & mask]; }

private:
    std::vector<float> data;
    size_t mask = 0, readPos = 0, writePos = 0;
};

class Engine
{
public:
    static constexpr int kFrame = 480;       // quadro do RNNoise a 48 kHz (10 ms)
    static constexpr int kMaxChannels = 2;

    Engine() = default;
    ~Engine();
    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;

    void prepare (double sampleRate, int numChannels);
    void reset();

    // Atraso total, em amostras do host, que o plugin deve informar ao DAW.
    int getLatencySamples() const { return latencySamples; }

    // clarity: 0..1   hushDb: 0..30   inputDb/outputDb: ganhos em dB
    void setParameters (float clarity, float hushDb, float inputDb, float outputDb);

    void process (float* const* channels, int numChannels, int numSamples);

    // Medidores (0..1 e dB), atualizados a cada quadro.
    float getVoiceProbability() const { return meterVoice; }
    float getReductionDb() const { return meterReduction; }

private:
    struct Channel
    {
        DenoiseState* rnn = nullptr;
        CubicResampler up, down;
        Biquad aaUp[2], aaDown[2];
        std::vector<float> in48;      // amostras a 48 kHz esperando completar um quadro
        std::vector<float> hostOut;   // saida reamostrada (temporario)
        Fifo wet;                     // saida processada na taxa do host
        std::vector<float> dry;       // linha de atraso do sinal original
        size_t dryPos = 0;
        float gateGain = 1.0f;
        int gateHold = 0;
    };

    void processChunk (Channel& ch, float* data, int len, bool first);
    void processFrame (Channel& ch);

    double hostRate = 48000.0;
    bool needsResample = false;
    int numCh = 0;
    int dryDelay = 0;
    int latencySamples = 0;
    int prefill = 0;
    Channel channels_[kMaxChannels];

    float mixAmount = 0.91f, hushDb = 9.0f;
    float inGain = 1.0f, outGain = 1.0f;
    float smoothedIn = 1.0f, smoothedOut = 1.0f, smoothedMix = 0.91f;
    float hushFloor = 0.35f;

    float meterVoice = 0.0f, meterReduction = 0.0f;
    float scratch48[kFrame] = {};
    float scratchOut[kFrame] = {};
    std::vector<float> chunkBuf;
};

} // namespace nitidez
