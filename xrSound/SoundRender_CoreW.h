#pragma once
#include "SoundRender_Core.h"
#include "WASAPIDeviceList.h"

class CSoundRender_CoreW : public CSoundRender_Core
{
    typedef CSoundRender_Core inherited;

    CWASAPIDeviceEnumerate* pEnumerator;

    struct SListener
    {
        Fvector position;
        Fvector orientation[2];
    };

    float fMasterVolume;

    SListener Listener;

#if defined(WINDOWS)
    bool EAXQuerySupport(bool isDeferred, const GUID* guid, u32 prop, void* val, u32 sz)
    {

    }

    bool EAXTestSupport(bool isDeferred)
    {

    }
#endif

protected:
#if defined(WINDOWS)
    void i_eax_set(const GUID* guid, u32 prop, void* val, u32 sz) override
    {

    }

    void i_eax_get(const GUID* guid, u32 prop, void* val, u32 sz) override
    {

    }
#endif

protected:
    void update_listener(const Fvector& P, const Fvector& D, const Fvector& N, float dt) override;

public:
    bool IsInputSupported = false;

    IMMDevice* pInputDevice;
    IMMDevice* pOutputDevice;

    IAudioClient* pInputClient;
    IAudioClient* pOutputClient;

    WASAPIDeviceDesc InputDesc;
    WASAPIDeviceDesc OutputDesc;

    /*
        100 ms latency
    */
    f32 InputLatency = 100.f;
    f32 OutputLatency = 100.f;

    CSoundRender_CoreW();
    virtual ~CSoundRender_CoreW();

    bool CheckInputSupport() { return IsInputSupported; }

    void _initialize() override;
    void _clear() override;
    void _restart() override;

    void set_master_volume(float f) override;

    const Fvector& listener_position() override { return Listener.position; }
};

extern CSoundRender_CoreW* SoundRenderW;
