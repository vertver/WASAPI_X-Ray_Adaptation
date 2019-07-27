/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base mixer impl.
 ****************************************************************/
#pragma once
#include "GameMixer.h"
#include "SoundRender_TargetW.h"

#define OPEN_FROM_SOUND 1
#include "../xrCore/Threading/Event.hpp"
#include "../xrCore/Threading/ThreadUtil.h"

/*
    Define input and output buffers count here
*/
#define INPUT_BUFFERS 3
#define OUTPUT_BUFFERS 3

class CWASAPIMixer
{
private:
    CSoundRender_CoreW* pParent;

    /*
        Used for internal stuff
    */
    bool IsLowLatency = false;

    HANDLE hCaptureEvent;
    HANDLE hRenderEvent;

    Event InitCaptureSync;
    Event InitRenderSync;
    Event DestroyCaptureSync;
    Event DestroyRenderSync;

    MiniWaveFormat miniInputFormat;
    MiniWaveFormat miniOutputFormat;

    IAudioCaptureClient* pCaptureClient;
    IAudioRenderClient* pRenderClient;

    /*
        Input and output buffer queue for processing to WASAPI Threads
    */
    xr_vector<f32> InputBuffer[INPUT_BUFFERS];
    f32* OutputBuffer[OUTPUT_BUFFERS];

    size_t CurrentInputBuffer = 0;
    size_t CurrentOutputBuffer = 0;

    void CaptureProcess(f32* pOutputBuffer, size_t FramesCount);
    void RenderProcess(f32* pOutputBuffer, size_t FramesCount);

public:
    CGameMixer Mixer;

    bool InitCaptureThread();
    bool InitRenderThread();

    void CaptureThreadProc();
    void RenderThreadProc();

    bool Initialize(CSoundRender_CoreW* ThisClass);
    void Destroy();

    void Stop(size_t TargetId);
    void Start(size_t TargetId);
    void Update(size_t TargetId);
    void FillParameters(size_t TargetId);
};

extern CWASAPIMixer OutMixer;
