/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT 
 ****************************************************************
 * Module Name: Base Game mixer impl.
 ****************************************************************/
#pragma once
#include "SoundRender_Core.h"

#define MAX_BUFFERS 64

struct EffectsRack
{
    f32 ReverbRatio;
    f32 ReverbMix;
};

struct BufferData
{
    bool IsQueued;
    size_t BufferIndex;
    size_t BufferFrames;        // Float frames 
    size_t BufferPosition;
    f32* pFloatData[8];
};

class CGameMixer
{
public:
    CGameMixer() : BuffersProcessed(0), BuffersCount(0), CurrentBuffer(0)
    {
        ZeroMemory(&MixerFormat, sizeof(MiniWaveFormat));

        for (size_t i = 0; i < MAX_BUFFERS; i++)
        {
            BaseBuffers[i] = nullptr;
        }
    }
  
    BufferData* BaseBuffers[MAX_BUFFERS];
    MiniWaveFormat MixerFormat;
    size_t BuffersProcessed;
    size_t BuffersCount;
    size_t CurrentBuffer;

    void GenerateBuffers(size_t BufferCount, size_t BufferSize);
    void DeleteBuffers(size_t Index);
    bool QueueBuffer(size_t Index);
    bool UnqueueBuffer(size_t Index);
    void UploadDataToBuffer(size_t Index, void* Data, size_t SizeInFrames, WAVEFORMATEX& WaveFormat);

    void Initalize(MiniWaveFormat OutputFormat);
    void Destroy();

    void Update(size_t UpdateFrames, f32* pOut);
};
