/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base render device impl.
 ****************************************************************/
#include "stdafx.h"
#include "WASAPIMixer.h"
#include "GameMixer.h"
#include <avrt.h>

CWASAPIMixer OutMixer;

bool CWASAPIMixer::InitRenderThread()
{
    WAVEFORMATEX* pWaveFormat = nullptr;

    if (SUCCEEDED(pParent->pOutputClient->GetMixFormat(&pWaveFormat)))
    {
        miniOutputFormat.IsCaptureDevice = false;
        miniOutputFormat.IsFloat = (pWaveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);

        /*
            Get float support by wave format
        */
        if (!miniOutputFormat.IsFloat)
        {
            if (pWaveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
            {
                WAVEFORMATEXTENSIBLE* pTmp = (WAVEFORMATEXTENSIBLE*)pWaveFormat;
                miniOutputFormat.IsFloat = !memcmp(&pTmp->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID));
                if (miniOutputFormat.IsFloat)
                {
                    miniOutputFormat.Bits = 32;
                }
            }
        }
        else
        {
            if (pWaveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
            {
                WAVEFORMATEXTENSIBLE* pTmp = (WAVEFORMATEXTENSIBLE*)pWaveFormat;
                miniOutputFormat.IsFloat = !!memcmp(&pTmp->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID));
            }
        }

        if (!miniOutputFormat.IsFloat)
        {
            miniOutputFormat.Bits = pWaveFormat->wBitsPerSample;
        }

        miniOutputFormat.Channels = pWaveFormat->nChannels;
        miniOutputFormat.SampleRate = pWaveFormat->nSamplesPerSec;

        UINT32 BufSize = 0;
        HRESULT hr = 0;
        REFERENCE_TIME refLatency = (miniOutputFormat.SampleRate / 1000.f) * pParent->InputLatency;
        if (!refLatency)
        {
            refLatency = (miniOutputFormat.SampleRate / 1000.f) * 100.f; // 100ms latency
        }

        if (FAILED((hr = pParent->pOutputClient->Initialize(
                        AUDCLNT_SHAREMODE_SHARED, 0, refLatency, 0, pWaveFormat, nullptr))))
        {
            /*
                It can be throwed if we set unaligned by sample rate buffer
            */
            if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
            {
                refLatency = 1000000;

                /*
                    Initalize device with default settings
                */
                hr = pParent->pOutputClient->Initialize(
                    AUDCLNT_SHAREMODE_SHARED, 0, refLatency, 0, pWaveFormat, nullptr);
                if (FAILED(hr))
                {
                    CoTaskMemFree(pWaveFormat);
                    return false;
                }
            }
        }

        /*
            If we can't get latency - set latency to 100ms
        */
        if (SUCCEEDED(pParent->pOutputClient->GetBufferSize(&BufSize)))
        {
            miniOutputFormat.FramesLatency = BufSize ? BufSize : miniOutputFormat.SampleRate / 10;
        }

        CoTaskMemFree(pWaveFormat);

        if (FAILED(pParent->pOutputClient->GetService(IID_PPV_ARGS(&pRenderClient))))
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    Mixer.Initalize(miniOutputFormat);

    return true;
}

void CWASAPIMixer::RenderThreadProc()
{
    DWORD dwTask = 0;
    HANDLE hAVRT = AvSetMmThreadCharacteristicsW(L"Pro Audio", &dwTask);
    if (!hAVRT || hAVRT == INVALID_HANDLE_VALUE)
        return;
        
    /*
        Set critical priority for render thread
    */
    AvSetMmThreadPriority(hAVRT, AVRT_PRIORITY_CRITICAL);

    if (!InitRenderThread())
    {
        InitRenderSync.Set();
        goto EndThread;
    }

    InitRenderSync.Set();

    for (size_t i = 0; i < OUTPUT_BUFFERS; i++)
    {
        OutputBuffer[i] = (f32*)xr_malloc(miniOutputFormat.FramesLatency * miniOutputFormat.Channels *
            (miniOutputFormat.IsFloat ? sizeof(f32) : sizeof(short)));
    }

    while (DestroyCaptureSync.Wait(GetSleepTime(miniOutputFormat.FramesLatency, miniOutputFormat.SampleRate)) == false)
    {
        DWORD dwSleep = 0;
        UINT32 StreamPadding = 0;
        HRESULT hr = 0;
        BYTE* pByte = NULL;

        /*
            Get count of samples who can be updated now
        */
        hr = pParent->pOutputClient->GetCurrentPadding(&StreamPadding);
        if (FAILED(hr))
        {
            goto EndThread;
        }

        INT32 AvailableFrames = miniOutputFormat.FramesLatency;
        AvailableFrames -= StreamPadding;
       
        /*
            Update and process data by mixer
        */
        Mixer.Update(miniOutputFormat.FramesLatency, &OutputBuffer[CurrentOutputBuffer][0]);
        RenderProcess(&OutputBuffer[CurrentOutputBuffer][0], miniOutputFormat.FramesLatency);

        while (AvailableFrames)
        {
            /*
                In this case, "GetBuffer" function can be failed if
                buffer length is too much for us
            */
            hr = pRenderClient->GetBuffer(AvailableFrames, &pByte);
            if (SUCCEEDED(hr))
            {
                /*
                    Process soundworker and copy data to main buffer
                */
                if (!pByte)
                {
                    continue;
                }

                /*
                    Convert and copy data to temp buffer
                */
                ConvertToStream(&OutputBuffer[CurrentOutputBuffer][miniOutputFormat.FramesLatency - AvailableFrames],
                    pByte, AvailableFrames, !!!miniOutputFormat.IsFloat);
            }
            else
            {
                /*
                    Don't try to destroy device if the buffer is unavailable
                */
                if (hr == AUDCLNT_E_BUFFER_TOO_LARGE)
                {
                    continue;
                }

                goto EndThread;
            }

            /*
                If we can't release buffer - close invalid host
            */
            hr = pRenderClient->ReleaseBuffer(AvailableFrames, 0);
            if (FAILED(hr))
            {
                goto EndThread;
            }

            hr = pParent->pOutputClient->GetCurrentPadding(&StreamPadding);
            if (FAILED(hr))
            {
                goto EndThread;
            }

            AvailableFrames = miniOutputFormat.FramesLatency;
            AvailableFrames -= StreamPadding;
        }

        if (CurrentOutputBuffer >= OUTPUT_BUFFERS - 1)
            CurrentOutputBuffer = 0;
        else
            CurrentOutputBuffer++;
    }

EndThread:
    Mixer.Destroy();

    for (size_t i = 0; i < OUTPUT_BUFFERS; i++)
    {
        xr_free(OutputBuffer[i]);
    }

    if (hAVRT)
    {
        AvRevertMmThreadCharacteristics(hAVRT);
    }
}
