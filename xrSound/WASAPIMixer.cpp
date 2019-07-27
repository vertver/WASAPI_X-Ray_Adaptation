/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base mixer impl.
 ****************************************************************/
#include "stdafx.h"
#include "WASAPIMixer.h"
#include <avrt.h>
#include <math.h>

#define signf(x) (signbit(x) ? -1 : 1)

void CWASAPIMixer::RenderProcess(f32* pOutputBuffer, size_t FramesCount) 
{      
    /*
        "Low-latency" mode means a buffer size lower or equ 50ms
    */
    if (!IsLowLatency)
    {
        /*
            Soft clipping for low clipping
        */
        for (size_t i = 0; i < FramesCount * miniOutputFormat.Channels; i++)
        {
            pOutputBuffer[i] = signf(pOutputBuffer[i]) * powf(atanf(powf(fabsf(pOutputBuffer[i]), 0.1f)), (1.0f / 0.1f));
        }
    }
    else
    {
        /*
            Fast hard clipping for low latency
        */
        for (size_t i = 0; i < FramesCount * miniOutputFormat.Channels; i++)
        {
            pOutputBuffer[i] = pOutputBuffer[i] > 1.0f ? 1.0f : pOutputBuffer[i] < -1.0f ? -1.0f : pOutputBuffer[i];
        }
    }
}

void CWASAPIMixer::CaptureProcess(f32* pOutputBuffer, size_t FramesCount)
{

}
