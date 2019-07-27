/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base sound target impl.
 ****************************************************************/
#pragma once
#include "SoundRender_CoreW.h"
#include "SoundRender_Target.h"

class CSoundRender_TargetW : public CSoundRender_Target
{
    CSoundRender_CoreW* CoreParent;
    using inherited = CSoundRender_Target;

    u32 buf_block;

    s32 cache_hw_volume;
    s32 cache_hw_freq;

public:
    CSoundRender_TargetW();
    virtual ~CSoundRender_TargetW();

    void _parent(CSoundRender_Core* pParent) 
    {
        CoreParent = (CSoundRender_CoreW*)pParent;
    }

    bool _initialize() override;
    void _destroy() override;
    void _restart() override;

    void start(CSoundRender_Emitter* E) override;
    void render() override;
    void rewind() override;
    void stop() override;
    void update() override;
    void fill_parameters() override;
    void fill_block(size_t BufferId);
};
