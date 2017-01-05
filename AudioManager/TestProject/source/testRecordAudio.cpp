// Example: record audio using portAudio library
#include <stdio.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "portaudio.h"
#include "common/pa_ringbuffer.h"
#include "common/pa_util.h"

#define PA_SAMPLE_TYPE paInt16
typedef short SAMPLE;
#define PA_NUM_CHANNEL (2)
#define PA_SAMPLE_RATE (44100)
#define PA_FRAME_PER_BUF (512)
#define PA_NUM_WRITE_PER_BUF (4)
#define PA_OUT_FILE "record_data.raw"

static int readCnt = 0;
static int writeCnt = 0;

struct paTestData {
    FILE *saveFile;
    PaUtilRingBuffer ringBuf;
    SAMPLE *ringBufData;
    int threadSyncFlag;
};

static unsigned NextPowerOf2(unsigned val)
{
    val--;
    val = (val >> 1) | val;
    val = (val >> 2) | val;
    val = (val >> 4) | val;
    val = (val >> 8) | val;
    val = (val >> 16) | val;
    return ++val;
}

int inputStreamCallback(const void *input, void *output, unsigned long frameCount, 
                        const PaStreamCallbackTimeInfo *timeInfo, 
                        PaStreamCallbackFlags statusFlags, void *userData);

void saveInputStream(void *userData);

int main(int argc, char *argv)
{
    PaStreamParameters inputParameters;
    PaStream *inputStream;
    PaError err = paNoError;
    paTestData data;

    // 500ms
    unsigned int numSamples = NextPowerOf2(PA_SAMPLE_RATE * 0.5 * PA_NUM_CHANNEL);
    unsigned int numBytes = sizeof(SAMPLE) * numSamples;
    data.ringBufData = (SAMPLE *)PaUtil_AllocateMemory(numBytes);
    if (PaUtil_InitializeRingBuffer(&data.ringBuf, sizeof(SAMPLE), numSamples, data.ringBufData) < 0) {
        std::cout << "Fail to initialize ring buffer!!!" << std::endl;
        return -1;
    }

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

    // create save thread
    std::thread save_thread(saveInputStream, &data);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    data.threadSyncFlag = 1;
    save_thread.join();
    Pa_CloseStream(inputStream);
    Pa_Terminate();
    if (data.ringBufData) {
        PaUtil_FreeMemory(data.ringBufData);
    }
    return 0;
}

int inputStreamCallback(const void *input, void *output, unsigned long frameCount,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags, void *userData)
{
    std::cout << "readCnt = " << readCnt++ << std::endl;
    //std::cout << "callback" << std::endl;
    //paTestData* data = (paTestData*)userData;
    //fwrite(input, sizeof(PA_SAMPLE_TYPE), frameCount*PA_NUM_CHANNEL, data->saveFile);

    paTestData* data = (paTestData *)userData;
    ring_buffer_size_t elementsWriteable = PaUtil_GetRingBufferWriteAvailable(&(data->ringBuf));
    ring_buffer_size_t elementsSum = frameCount * PA_NUM_CHANNEL;
    ring_buffer_size_t elementsToWrite = elementsWriteable < elementsSum ? elementsWriteable : elementsSum;

    PaUtil_WriteRingBuffer(&(data->ringBuf), input, elementsToWrite);

    return paContinue;
}

void saveInputStream(void *userData)
{
    paTestData *data = (paTestData *)userData;

    data->threadSyncFlag = 0;

    while (1) {
        std::cout << "writeCnt = " << writeCnt++ << std::endl;
        ring_buffer_size_t elementsInBuffer = PaUtil_GetRingBufferReadAvailable(&(data->ringBuf));
        if (elementsInBuffer >= data->ringBuf.bufferSize / PA_NUM_WRITE_PER_BUF ||
            data->threadSyncFlag) {
            void *ptr[2] = { 0 };
            ring_buffer_size_t size2[2] = { 0 };
            ring_buffer_size_t elementsRead = PaUtil_GetRingBufferReadRegions(&(data->ringBuf), 
                                                elementsInBuffer,
                                                ptr, size2, ptr + 1, size2 + 1);
            if (elementsRead > 0) {
                for (int i = 0; i < 2; i++) {
                    fwrite(ptr[i], data->ringBuf.elementSizeBytes, size2[i], data->saveFile);
                }
                PaUtil_AdvanceRingBufferReadIndex(&(data->ringBuf), elementsRead);
            }
            if (data->threadSyncFlag) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    data->threadSyncFlag = 0;
}
