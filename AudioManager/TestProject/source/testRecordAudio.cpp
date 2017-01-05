// Example: record audio using portAudio library
#include <stdio.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "portaudio.h"
#include "common/pa_ringbuffer.h"

#define PA_SAMPLE_TYPE paFloat32
#define PA_NUM_CHANNEL (2)
#define PA_SAMPLE_RATE (44100)
#define PA_FRAME_PER_BUF (512)
#define PA_OUT_FILE "record_data.raw"

struct paTestData {
    FILE *saveFile;
};

int inputStreamCallback(const void *input, void *output, unsigned long frameCount, 
                        const PaStreamCallbackTimeInfo *timeInfo, 
                        PaStreamCallbackFlags statusFlags, void *userData);

int main(int argc, char *argv)
{
    PaStreamParameters inputParameters;
    PaStream *inputStream;
    PaError err = paNoError;
    paTestData data;

    err = Pa_Initialize();
    if (err != paNoError) {
        std::cout << "Fail to initialize!!!" << std::endl;
        return -1;
    }

    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        std::cout << "Not found input device!!!" << std::endl;
        return - 1;
    }
    inputParameters.channelCount = PA_NUM_CHANNEL;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&inputStream, &inputParameters, NULL, PA_SAMPLE_RATE,
                        PA_FRAME_PER_BUF, paClipOff, inputStreamCallback, &data);
    fopen_s(&(data.saveFile), PA_OUT_FILE, "wb");
    Pa_StartStream(inputStream);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    Pa_CloseStream(inputStream);
    Pa_Terminate();
    return 0;
}

int inputStreamCallback(const void *input, void *output, unsigned long frameCount,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags, void *userData)
{
    std::cout << "callback" << std::endl;
    paTestData* data = (paTestData*)userData;
    fwrite(input, sizeof(PA_SAMPLE_TYPE), frameCount*PA_NUM_CHANNEL, data->saveFile);

    return paContinue;
}