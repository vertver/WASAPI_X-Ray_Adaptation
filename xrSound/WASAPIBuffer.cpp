/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base buffer impl.
 ****************************************************************/
#include "stdafx.h"
#include "WASAPIMixer.h"
#include <avrt.h>

/*
    C-like thread procs for WASAPI support
*/
void WASAPICaptureThreadProc(void* pProc)
{
    CWASAPIMixer* pThis = (CWASAPIMixer*)pProc;
    pThis->CaptureThreadProc();
}

void WASAPIRenderThreadProc(void* pProc)
{
    CWASAPIMixer* pThis = (CWASAPIMixer*)pProc;
    pThis->RenderThreadProc();
}

bool CWASAPIMixer::Initialize(CSoundRender_CoreW* ThisClass)
{    
    pParent = ThisClass;

    if (!Threading::SpawnThread(WASAPIRenderThreadProc, "X-Ray WASAPI Render Thread", 0, this))
    {
        return false;
    }

    if (!Threading::SpawnThread(WASAPICaptureThreadProc, "X-Ray WASAPI Capture Thread", 0, this))
    {
        pParent->IsInputSupported = false;
    }

    InitRenderSync.Wait();
    if (!pRenderClient)
    {
        DestroyRenderSync.Set();
        return false;
    }

    InitCaptureSync.Wait();
    if (!pCaptureClient)
    {
        DestroyCaptureSync.Set();
        pParent->IsInputSupported = false;
    }

    if (FAILED(pParent->pOutputClient->Start()))
    {
        DestroyRenderSync.Set();
        return false;
    }

    if (FAILED(pParent->pInputClient->Start()))
    {
        DestroyCaptureSync.Set();
        pParent->IsInputSupported = false;
    }

    return true;
}

void CWASAPIMixer::Destroy()
{   
    DestroyRenderSync.Set();
    DestroyCaptureSync.Set();
}
