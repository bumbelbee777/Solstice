#pragma once

#include <Core/Audio/Audio.hxx>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <cstdint>

namespace DiscoDemo {

    class BassDrop {
    public:
        static void Initialize() {
            // Check if file exists
            const char* filename = "BassDrop.wav";
            std::ifstream check(filename);
            if (!check.good()) {
                GenerateWav(filename);
            }
        }

        static void Play() {
            Solstice::Core::Audio::AudioManager::Instance().PlaySound("BassDrop.wav");
        }

    private:
        static void WriteInt32(std::ofstream& stream, int32_t value) {
            stream.write(reinterpret_cast<const char*>(&value), 4);
        }

        static void WriteInt16(std::ofstream& stream, int16_t value) {
            stream.write(reinterpret_cast<const char*>(&value), 2);
        }

        static void GenerateWav(const char* filename) {
            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open()) return;

            // Audio parameters
            const int sampleRate = 44100;
            const int channels = 1;
            const int bitsPerSample = 16;
            const float durationSeconds = 3.0f;
            const int numSamples = static_cast<int>(sampleRate * durationSeconds);

            // WAV Header
            file << "RIFF";
            WriteInt32(file, 36 + numSamples * 2); // ChunkSize
            file << "WAVE";
            file << "fmt ";
            WriteInt32(file, 16); // Subchunk1Size (16 for PCM)
            WriteInt16(file, 1);  // AudioFormat (1 for PCM)
            WriteInt16(file, channels);
            WriteInt32(file, sampleRate);
            WriteInt32(file, sampleRate * channels * bitsPerSample / 8); // ByteRate
            WriteInt16(file, channels * bitsPerSample / 8); // BlockAlign
            WriteInt16(file, bitsPerSample);
            file << "data";
            WriteInt32(file, numSamples * 2); // Subchunk2Size

            // Generate Sound Data (Dubstep Bass Drop)
            for (int i = 0; i < numSamples; ++i) {
                float t = static_cast<float>(i) / sampleRate;
                
                // Frequency sweep (Drop): 150Hz -> 30Hz
                float startFreq = 150.0f;
                float endFreq = 30.0f;
                float freq = startFreq + (endFreq - startFreq) * (t / durationSeconds);

                // Wobble LFO: 5Hz -> 15Hz
                float lfoFreq = 5.0f + 10.0f * (t / durationSeconds);
                float lfo = std::sin(2.0f * 3.14159f * lfoFreq * t);
                
                // Waveform: Sine with wobble amplitude modulation + slight distortion
                float sample = std::sin(2.0f * 3.14159f * freq * t);
                
                // Apply wobble
                sample *= (0.5f + 0.5f * lfo);

                // Distortion (square-ish clipping)
                sample = std::tanh(sample * 5.0f);

                // Volume envelope (Fade out at end)
                float vol = 1.0f;
                if (t > durationSeconds - 0.5f) {
                    vol = (durationSeconds - t) / 0.5f;
                }
                
                // Master volume
                sample *= 0.8f * vol;

                int16_t pcm = static_cast<int16_t>(sample * 32767.0f);
                WriteInt16(file, pcm);
            }

            file.close();
        }
    };
}
