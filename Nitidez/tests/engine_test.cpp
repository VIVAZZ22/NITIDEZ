#define _USE_MATH_DEFINES // MSVC: habilita M_PI
// Teste do motor do Nitidez, sem JUCE.
// g++ -O2 -std=c++17 tests/engine_test.cpp Source/NitidezEngine.cpp third_party/rnnoise/src/*.c ...
#include "../Source/NitidezEngine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using nitidez::Engine;

static std::vector<float> makeSignal (double fs, double seconds, bool withVoice, unsigned seed, float noiseLevel)
{
    const size_t n = (size_t) (fs * seconds);
    std::vector<float> x (n);
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);

    // ruido "de sala": branco filtrado (mais grave), com RMS = noiseLevel
    std::vector<float> noise (n);
    {
        float y = 0.0f; double e = 0.0;
        for (size_t i = 0; i < n; ++i) { y = 0.9f * y + 0.1f * d (rng); noise[i] = y; e += (double) y * y; }
        const float k = noiseLevel / (float) std::sqrt (e / (double) n);
        for (auto& v : noise) v *= k;
    }

    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / fs;
        double v = 0.0;
        if (withVoice && t > 1.0 && t < seconds - 1.0)
        {
            const double env = 0.6 + 0.4 * std::sin (2 * M_PI * 3.0 * t);
            v = 0.20 * env * std::sin (2 * M_PI * 140.0 * t)
              + 0.10 * env * std::sin (2 * M_PI * 420.0 * t)
              + 0.07 * env * std::sin (2 * M_PI * 1260.0 * t)
              + 0.04 * env * std::sin (2 * M_PI * 2520.0 * t);
        }
        x[i] = (float) v + noise[i];
    }
    return x;
}

static std::vector<float> run (Engine& e, std::vector<float> x, int block, int channels = 1)
{
    std::vector<float> out (x.size());
    std::vector<float> r (x.size());
    for (size_t off = 0; off < x.size(); off += (size_t) block)
    {
        const int len = (int) std::min ((size_t) block, x.size() - off);
        float* ptrs[2] = { x.data() + off, x.data() + off };
        std::vector<float> second (x.begin() + (long) off, x.begin() + (long) off + len);
        if (channels == 2) ptrs[1] = second.data();
        e.process (ptrs, channels, len);
        for (int i = 0; i < len; ++i) out[off + (size_t) i] = ptrs[0][i];
    }
    return out;
}

static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0;
    for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (b - a));
}

int main()
{
    int failures = 0;
    auto check = [&] (bool ok, const char* msg) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALHOU", msg); if (! ok) ++failures; };

    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 32000.0 };

    for (double fs : rates)
    {
        std::printf ("== %.0f Hz ==\n", fs);
        const double secs = 6.0;
        auto noisy = makeSignal (fs, secs, true, 1, 0.02f);
        auto clean = makeSignal (fs, secs, true, 1, 0.0f);

        Engine e;
        e.prepare (fs, 1);
        e.setParameters (1.0f, 0.0f, 0.0f, 0.0f);
        e.reset();
        const int L = e.getLatencySamples();
        auto y = run (e, noisy, 480);

        // 1) alinhamento: a saida processada deve acompanhar a voz limpa atrasada em L amostras
        double bestErr = 1e9; int bestLag = -999;
        const size_t a = (size_t) (fs * 2.0), b = (size_t) (fs * 4.0);
        for (int lag = L - 40; lag <= L + 40; ++lag)
        {
            double err = 0, ref = 0;
            for (size_t i = a; i < b; ++i)
            {
                const double d = (double) y[i] - (double) clean[i - (size_t) lag];
                err += d * d; ref += (double) clean[i - (size_t) lag] * clean[i - (size_t) lag];
            }
            if (err / ref < bestErr) { bestErr = err / ref; bestLag = lag; }
        }
        std::printf ("  latencia informada: %d amostras (%.1f ms); melhor alinhamento medido: %d\n", L, 1000.0 * L / fs, bestLag);
        check (std::abs (bestLag - L) <= 2, "latencia informada bate com a medida (+-2 amostras)");

        // 2) ruido: trecho final, so com ruido, deve cair bastante
        const size_t t0 = (size_t) (fs * (secs - 0.7)), t1 = (size_t) (fs * (secs - 0.1));
        const double nin = rms (noisy, t0, t1);
        const double nout = rms (y, t0, t1);
        const double redDb = 20.0 * std::log10 (nout / nin);
        std::printf ("  ruido: %.4f -> %.4f (%.1f dB)\n", nin, nout, redDb);
        check (redDb < -15.0, "reduz o ruido de fundo em mais de 15 dB");

        // 3) voz preservada
        const double vin = rms (clean, a, b);
        const double vout = rms (y, a + (size_t) L, b + (size_t) L);
        std::printf ("  voz: %.4f -> %.4f (%.1f dB)\n", vin, vout, 20.0 * std::log10 (vout / vin));
        check (std::abs (20.0 * std::log10 (vout / vin)) < 3.0, "preserva o nivel da voz (+-3 dB)");

        // 4) invariancia ao tamanho do bloco do host
        Engine e2; e2.prepare (fs, 1); e2.setParameters (1.0f, 0.0f, 0.0f, 0.0f); e2.reset();
        Engine e3; e3.prepare (fs, 1); e3.setParameters (1.0f, 0.0f, 0.0f, 0.0f); e3.reset();
        auto y2 = run (e2, noisy, 64);
        auto y3 = run (e3, noisy, 1777);
        double maxd = 0;
        for (size_t i = 0; i < y2.size(); ++i) maxd = std::max (maxd, (double) std::abs (y2[i] - y3[i]));
        check (maxd < 1e-6, "saida identica com blocos de 64 e de 1777 amostras");
        check (maxd < 1e-6 && std::abs (rms (y2, 0, y2.size()) - rms (y, 0, y.size())) < 1e-6, "idem com bloco de 480");

        // 5) Clarity = 0 -> sinal original, apenas atrasado
        Engine e4; e4.prepare (fs, 1); e4.setParameters (0.0f, 0.0f, 0.0f, 0.0f); e4.reset();
        auto y4 = run (e4, noisy, 256);
        double maxErr = 0;
        for (size_t i = (size_t) L; i < y4.size(); ++i) maxErr = std::max (maxErr, (double) std::abs (y4[i] - noisy[i - (size_t) L]));
        check (maxErr < 1e-5, "Clarity 0 devolve o original (atrasado pela latencia)");

        // 6) sem NaN/inf e sem estouro
        bool finite = true; float peak = 0;
        for (float v : y) { if (! std::isfinite (v)) finite = false; peak = std::max (peak, std::abs (v)); }
        check (finite && peak < 2.0f, "sem NaN/inf nem estouro");
    }

    {
        std::printf ("== estereo 44100 Hz + Hush ==\n");
        const double fs = 44100.0;
        auto noisy = makeSignal (fs, 5.0, true, 3, 0.02f);
        Engine e; e.prepare (fs, 2); e.setParameters (1.0f, 24.0f, 0.0f, 0.0f); e.reset();
        auto y = run (e, noisy, 441, 2);
        const double nout = rms (y, (size_t) (fs * 4.3), (size_t) (fs * 4.9));
        const double nin = rms (noisy, (size_t) (fs * 4.3), (size_t) (fs * 4.9));
        std::printf ("  ruido entre frases com Hush 24 dB: %.1f dB\n", 20 * std::log10 (nout / nin));
        check (20 * std::log10 (nout / nin) < -30.0, "Hush aprofunda a reducao entre frases");
        std::printf ("  medidores: voz=%.2f reducao=%.1f dB\n", e.getVoiceProbability(), e.getReductionDb());
    }

    std::printf ("\n%s\n", failures == 0 ? "TODOS OS TESTES PASSARAM" : "HOUVE FALHAS");
    return failures == 0 ? 0 : 1;
}
