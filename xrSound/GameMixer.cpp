/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base Game mixer impl.
 ****************************************************************/
#include "stdafx.h"
#include "GameMixer.h"
#include "../xrCore/xrMemory.h"

void CGameMixer::Initalize(MiniWaveFormat OutputFormat) { memcpy(&MixerFormat, &OutputFormat, sizeof(MiniWaveFormat)); }

void CGameMixer::Destroy()
{
    for (size_t i = 0; i < MAX_BUFFERS; i++)
    {
        if (BaseBuffers[i])
        {
            for (size_t u = 0; u < 8; u++)
            {
                xr_free(BaseBuffers[i]->pFloatData[u]);
            }

            delete BaseBuffers[i];
        }
    }
}

void CGameMixer::GenerateBuffers(size_t BufferCount, size_t BufferSize)
{
    for (size_t i = 0; i < BufferCount; i++)
    {
        BaseBuffers[i] = (BufferData*)xr_malloc(sizeof(BufferData));
        ZeroMemory(BaseBuffers[i], sizeof(BufferData));

        /*
            Allocate buffer for every channel
        */
        for (size_t u = 0; u < MixerFormat.Channels; u++)
        {
            BaseBuffers[i]->pFloatData[u] = (f32*)xr_malloc(BufferSize * sizeof(f32));
            ZeroMemory(BaseBuffers[i]->pFloatData[u], BufferSize * sizeof(f32));
        }

        BaseBuffers[i]->BufferFrames = BufferSize;
        BaseBuffers[i]->BufferIndex = i;
        BuffersCount++;
        if (BuffersProcessed) BuffersProcessed--;
    }
}

void CGameMixer::DeleteBuffers(size_t Index)
{
    for (size_t u = 0; u < MixerFormat.Channels; u++)
    {
        xr_free(BaseBuffers[Index]->pFloatData[u]);
    }

    xr_free(BaseBuffers[Index]);

    if (BuffersCount)
        BuffersCount--;
}

bool CGameMixer::QueueBuffer(size_t Index)
{ 
    if (BaseBuffers[Index]->IsQueued) 
        return false;

    BaseBuffers[Index]->IsQueued = true;
    return true;
}

bool CGameMixer::UnqueueBuffer(size_t Index) 
{
    if (!BaseBuffers[Index]->IsQueued)
        return false;

    BaseBuffers[Index]->IsQueued = false;
    return true;
}

void CGameMixer::UploadDataToBuffer(size_t Index, void* Data, size_t SizeInFrames, WAVEFORMATEX& WaveFormat)
{
    size_t StreamChannels = WaveFormat.nChannels;

    if (BaseBuffers[Index])
    {
        if (BaseBuffers[Index]->BufferFrames >= SizeInFrames)
        {
            /*
                Copy float without any conversion
            */
            if (WaveFormat.wFormatTag == 3 || WaveFormat.wBitsPerSample == 32)
            {
                f32* pFloat = (f32*)Data;

                for (size_t i = 0; i < SizeInFrames * StreamChannels; i++)
                {
                    BaseBuffers[Index]->pFloatData[i % StreamChannels][i / StreamChannels] = pFloat[i];
                }
            }
            else
            {
                /*
                    Convert to float and copy to buffer storage
                */
                if (WaveFormat.wFormatTag == 16)
                {
                    short* pShort = (short*)Data;

                    for (size_t i = 0; i < SizeInFrames * StreamChannels; i++)
                    {
                        BaseBuffers[Index]->pFloatData[i % StreamChannels][i / StreamChannels] =
                            BaseToFloat(&pShort[i], 1);
                    }
                }
            }
        }

        BaseBuffers[Index]->BufferPosition = 0;
    }
}

/*
    Current buffer
*/
void CGameMixer::Update(size_t UpdateFrames, f32* pOut)
{
    if (BaseBuffers[CurrentBuffer])
    {
        /*
            We can't play unqueued or empty buffer
        */
        if (!!BaseBuffers[CurrentBuffer]->pFloatData[0] && BaseBuffers[CurrentBuffer]->IsQueued &&
            BaseBuffers[CurrentBuffer]->BufferFrames > UpdateFrames)
        {
            size_t ChannelsCount = 0;
            size_t MixChannelsCount = MixerFormat.Channels;
            size_t EndFrames = BaseBuffers[CurrentBuffer]->BufferFrames - BaseBuffers[CurrentBuffer]->BufferPosition;
            size_t NeedySamples = UpdateFrames > EndFrames ? EndFrames : UpdateFrames;

            if (!NeedySamples)
                return;

            /*
                Get count of all channels in buffer
            */
            for (size_t u = 0; u < 8; u++)
            {
                if (!BaseBuffers[CurrentBuffer]->pFloatData[u])
                    break;

                ChannelsCount++;
            }

            if (ChannelsCount >= 2)
            {
                if (MixChannelsCount == 1)
                {
                    for (size_t o = 0; o < NeedySamples; o++)
                    {
                        /*
                            Get middle channel from stereo or surround signal
                        */
                        pOut[o] += (BaseBuffers[CurrentBuffer]->pFloatData[0][o] +
                                       BaseBuffers[CurrentBuffer]->pFloatData[1][o]) * 0.5f;
                    }
                }
                else if (MixChannelsCount >= 2)
                {
                    for (size_t o = 0; o < NeedySamples; o++)
                    {
                        pOut[o * MixChannelsCount] += BaseBuffers[CurrentBuffer]->pFloatData[0][o];
                        pOut[o * MixChannelsCount + 1] += BaseBuffers[CurrentBuffer]->pFloatData[1][o];
                    }
                }
            }
            else
            {
                if (MixChannelsCount == 1)
                {
                    for (size_t o = 0; o < NeedySamples; o++)
                    {
                        /*
                            Get middle channel from stereo or surround signal
                        */
                        pOut[o] += BaseBuffers[CurrentBuffer]->pFloatData[0][o];
                    }
                }
                else if (MixChannelsCount >= 2)
                {
                    for (size_t o = 0; o < NeedySamples; o++)
                    {
                        f32 fl = BaseBuffers[CurrentBuffer]->pFloatData[0][o];
                        pOut[o] += fl;
                        pOut[o * MixChannelsCount + 1] += fl;
                    }
                }
            }

            BaseBuffers[CurrentBuffer]->BufferPosition += NeedySamples;
        }

        if (!(BaseBuffers[CurrentBuffer]->BufferFrames - BaseBuffers[CurrentBuffer]->BufferPosition))
        {
            BaseBuffers[CurrentBuffer]->IsQueued = false;
            CurrentBuffer++;
            if (CurrentBuffer >= BuffersCount)
            {
                CurrentBuffer = 0;
            }
        }
        
        BuffersProcessed++;
    }
}
