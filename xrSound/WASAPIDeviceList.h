/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base enumerator device impl.
 ****************************************************************/
#pragma once
#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <devpkey.h>
#include <Functiondiscoverykeys_devpkey.h>
#include "xrCore/xr_token.h"
#include "SoundRender_Core.h"

struct WASAPIDeviceDesc
{
    MiniWaveFormat DeviceFormat;
    string256 DeviceName;
    string256 DeviceGUID;
};

class CWASAPIDeviceEnumerate
{
    xr_vector<WASAPIDeviceDesc> InputDevicesVector;
    xr_vector<WASAPIDeviceDesc> OutputDevicesVector;

public:
    string256 DefaultInputDeviceGUID;
    string256 DefaultOutputDeviceGUID;
    u32 CurrentDeviceId;

    void Enumerate();

    CWASAPIDeviceEnumerate()
    {
        ZeroMemory(DefaultInputDeviceGUID, sizeof(string256));
        ZeroMemory(DefaultOutputDeviceGUID, sizeof(string256));
    }

    ~CWASAPIDeviceEnumerate() 
    {
        for (int i = 0; snd_devices_token[i].name; i++)
            xr_free(snd_devices_token[i].name);

        xr_free(snd_devices_token);
        snd_devices_token = nullptr;
    }

    u32 GetInNumDevices() const { return InputDevicesVector.size(); }
    u32 GetOutNumDevices() const { return OutputDevicesVector.size(); }
    const WASAPIDeviceDesc& GetInDeviceDesc(u32 DeviceIndex) { return InputDevicesVector[DeviceIndex]; }
    const WASAPIDeviceDesc& GetOutDeviceDesc(u32 DeviceIndex) { return OutputDevicesVector[DeviceIndex]; }
    void GetInDeviceName(u32 DeviceIndex, string256& DeviceName) const;
    void GetOutDeviceName(u32 DeviceIndex, string256& DeviceName) const;
};
