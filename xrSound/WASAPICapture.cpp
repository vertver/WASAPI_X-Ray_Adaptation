/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base capture device impl.
 ****************************************************************/
#include "stdafx.h"
#include "WASAPIMixer.h"
#include <avrt.h>

bool CWASAPIMixer::InitCaptureThread()
{
    WAVEFORMATEX* pWaveFormat = nullptr;

    if (SUCCEEDED(pParent->pInputClient->GetMixFormat(&pWaveFormat)))
    {
        miniInputFormat.IsCaptureDevice = true;
        miniInputFormat.IsFloat = (pWaveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);

        /*
            Get float support by wave format
        */
        if (!miniInputFormat.IsFloat)
        {
            if (pWaveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
            {
                WAVEFORMATEXTENSIBLE* pTmp = (WAVEFORMATEXTENSIBLE*)pWaveFormat;
                miniInputFormat.IsFloat = !memcmp(&pTmp->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID));
                if (miniInputFormat.IsFloat)
                {
                    miniInputFormat.Bits = 32;
                }
            }
        }
        else
        {
            if (pWaveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
            {
                WAVEFORMATEXTENSIBLE* pTmp = (WAVEFORMATEXTENSIBLE*)pWaveFormat;
                miniInputFormat.IsFloat = !!memcmp(&pTmp->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID));
            }
        }

        if (!miniInputFormat.IsFloat)
        {
            miniInputFormat.Bits = pWaveFormat->wBitsPerSample;
        }

        miniInputFormat.Channels = pWaveFormat->nChannels;
        miniInputFormat.SampleRate = pWaveFormat->nSamplesPerSec;

        UINT32 BufSize = 0;
        HRESULT hr = 0;
        REFERENCE_TIME refLatency = (miniInputFormat.SampleRate / 1000.f) * pParent->InputLatency;
        if (!refLatency)
        {
            refLatency = (miniInputFormat.SampleRate / 1000.f) * 100.f; // 100ms latency
        }

        if (FAILED((hr = pParent->pInputClient->Initialize(
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
                hr =
                    pParent->pInputClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, refLatency, 0, pWaveFormat, nullptr);
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
        if (pParent->pInputClient->GetBufferSize(&BufSize))
        {
            miniInputFormat.FramesLatency = BufSize ? BufSize : miniInputFormat.SampleRate / 10;
        }

        CoTaskMemFree(pWaveFormat);

        if (FAILED(pParent->pInputClient->GetService(IID_PPV_ARGS(&pCaptureClient))))
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

void CWASAPIMixer::CaptureThreadProc()
{
    DWORD dwTask = 0;
    DWORD flags = 0;
    UINT32 dwFramesAvailable = 0;
    UINT32 dwPacketSize = 0;
    HRESULT hr = 0;
    HANDLE hAVRT = AvSetMmThreadCharacteristicsW(L"Capture", &dwTask);
    BYTE* pData = nullptr;

    if (!hAVRT || hAVRT == INVALID_HANDLE_VALUE)
        return;

    AvSetMmThreadPriority(hAVRT, AVRT_PRIORITY_NORMAL);

    if (!InitCaptureThread())
    {
        InitCaptureSync.Set();
        goto EndThread;
    }

    InitCaptureSync.Set();

    /*
        While we don't exit - process capture
    */
    while (DestroyCaptureSync.Wait(GetSleepTime(miniInputFormat.FramesLatency, miniInputFormat.SampleRate)) == false)
    {
        /*
            #TODO: Check if the buffer can be other size than base
        */
        InputBuffer[CurrentInputBuffer].clear();
        InputBuffer[CurrentInputBuffer].reserve(miniInputFormat.FramesLatency * miniInputFormat.Channels);

        hr = pCaptureClient->GetNextPacketSize(&dwPacketSize);

        while (dwPacketSize)
        {
            /*
                Get the available data in the shared buffer.
            */
            hr = pCaptureClient->GetBuffer(&pData, &dwFramesAvailable, &flags, nullptr, nullptr);

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
            {
                pData = nullptr;
            }
            else
            {
                /*
                    Convert input format to float
                */
                if (miniInputFormat.IsFloat)
                {
                    f32* pFloat = (f32*)(pData);

                    for (size_t i = 0; i < dwFramesAvailable * miniInputFormat.Channels; i++)
                    {
                        InputBuffer[CurrentInputBuffer].push_back(pFloat[i]);
                    }
                }
                else
                {
                    short* pShort = (short*)(pData);

                    for (size_t i = 0; i < dwFramesAvailable * miniInputFormat.Channels; i++)
                    {
                        InputBuffer[CurrentInputBuffer].push_back(BaseToFloat((void*)&pShort[i], 1));
                    }
                }

                CaptureProcess(&InputBuffer[CurrentInputBuffer][0], dwFramesAvailable);
            }

            hr = pCaptureClient->ReleaseBuffer(dwFramesAvailable);
            hr = pCaptureClient->GetNextPacketSize(&dwPacketSize);
        }

        if (CurrentInputBuffer >= INPUT_BUFFERS - 1)
            CurrentInputBuffer = 0;
        else
            CurrentInputBuffer++;
    }

    for (size_t i = 0; i < INPUT_BUFFERS; i++)
    {
        InputBuffer[i].clear();
    }

    pParent->pInputClient->Stop();
    pParent->pInputClient->Reset();

EndThread:
    if (hAVRT)
    {
        AvRevertMmThreadCharacteristics(hAVRT);
    }
}
