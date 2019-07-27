/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base sound target impl.
 ****************************************************************/
#include "stdafx.h"
#include "SoundRender_TargetW.h"
#include "SoundRender_Emitter.h"
#include "SoundRender_Source.h"
#include "WASAPIMixer.h"

xr_vector<u8> g_target_temp_dataw;

CSoundRender_TargetW::CSoundRender_TargetW()
{

}

CSoundRender_TargetW::~CSoundRender_TargetW()
{

}

bool CSoundRender_TargetW::_initialize()
{
    inherited::_initialize();
    return true; 
}

void CSoundRender_TargetW::_destroy() 
{ 
    for (size_t i = 0; i <  sdef_target_count; i++)
    {
        OutMixer.Mixer.DeleteBuffers(i);
    }
}

void CSoundRender_TargetW::_restart()
{
    _destroy();
    _initialize();
}

void CSoundRender_TargetW::start(CSoundRender_Emitter* E)
{
    inherited::start(E);

    OutMixer.Mixer.GenerateBuffers(
        sdef_target_count, (size_t)((float)sdef_target_block * ((float)E->source()->m_wformat.nSamplesPerSec / 1000.f)));

    // Calc storage
    buf_block = sdef_target_block * E->source()->m_wformat.nAvgBytesPerSec / 1000;
    g_target_temp_dataw.resize(buf_block);
}

void CSoundRender_TargetW::render()
{
    for (u32 buf_idx = 0; buf_idx < sdef_target_count; buf_idx++)
    {
        fill_block(buf_idx);
        OutMixer.Mixer.QueueBuffer(buf_idx);
    }

    inherited::render();
}

void CSoundRender_TargetW::rewind()
{
    inherited::rewind();

    for (u32 buf_idx = 0; buf_idx < sdef_target_count; buf_idx++)
    {
        fill_block(buf_idx);
        OutMixer.Mixer.QueueBuffer(buf_idx);
    }
}

void CSoundRender_TargetW::stop() 
{ 
    for (size_t i = 0; i < sdef_target_count; i++)
    {
        OutMixer.Mixer.DeleteBuffers(i);
    }

    inherited::stop(); 
}

void CSoundRender_TargetW::update()
{
    inherited::update();
    size_t ProcessedBufs = OutMixer.Mixer.BuffersProcessed;
    size_t Index = 0;

    while (ProcessedBufs > 0)
    {
        if (Index >= OutMixer.Mixer.BuffersCount) break;
        if (!OutMixer.Mixer.UnqueueBuffer(Index))
        {
            Index++;
            continue;
        }

        fill_block(Index);

        Index++;
        ProcessedBufs--;
    }
}

void CSoundRender_TargetW::fill_parameters()
{ 
    inherited::fill_parameters(); 
}

void CSoundRender_TargetW::fill_block(size_t BufferId)
{ 
    m_pEmitter->fill_block(&g_target_temp_dataw.front(), buf_block);
    OutMixer.Mixer.UploadDataToBuffer(BufferId, &g_target_temp_dataw.front(),
        buf_block / m_pEmitter->source()->m_wformat.nChannels / (m_pEmitter->source()->m_wformat.wBitsPerSample / 8),
        m_pEmitter->source()->m_wformat);
    OutMixer.Mixer.QueueBuffer(BufferId);
}
