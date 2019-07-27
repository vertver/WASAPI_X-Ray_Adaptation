/****************************************************************
 * Copyright (C) Vertver (Suirless), 2019. All rights reserved.
 * WASAPI Mixer implementation for X-Ray Engine
 * License: MIT
 ****************************************************************
 * Module Name: Base enumerator device impl.
 ****************************************************************/
#include "stdafx.h"
#include "WASAPIDeviceList.h"
#include "../xrCore/Text/StringConversion.hpp"

void CWASAPIDeviceEnumerate::Enumerate()
{
    UINT CountOfInputs = 0;
    UINT CountOfOutputs = 0;
    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDeviceCollection* pCaptureEndPoints = nullptr;
    IMMDeviceCollection* pRenderEndPoints = nullptr;

    InputDevicesVector.clear();
    OutputDevicesVector.clear();

    if (SUCCEEDED(CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&deviceEnumerator))))
    {
        deviceEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCaptureEndPoints);
        deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pRenderEndPoints);

        /*
            Get default output device GUID
        */
        {
            string256 lpNewString = {};
            LPWSTR lpString = nullptr;
            IMMDevice* pDeviceThis = nullptr;
            deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDeviceThis);

            if (SUCCEEDED(pDeviceThis->GetId(&lpString)))
            {
                // convert to UTF-8
                if (WideCharToMultiByte(CP_UTF8, 0, lpString, -1, lpNewString, 256, nullptr, nullptr))
                {
                    CoTaskMemFree(lpString);
                    xr_strcpy(DefaultOutputDeviceGUID, sizeof(string256), lpNewString);
                }
            }

            _RELEASE(pDeviceThis);
        }

        /*
            Get default input device GUID
        */
        {
            string256 lpNewString = {};
            LPWSTR lpString = nullptr;
            IMMDevice* pDeviceThis = nullptr;
            deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDeviceThis);

            if (SUCCEEDED(pDeviceThis->GetId(&lpString)))
            {
                // convert to UTF-8
                if (WideCharToMultiByte(CP_UTF8, 0, lpString, -1, lpNewString, 256, nullptr, nullptr))
                {
                    CoTaskMemFree(lpString);
                    xr_strcpy(DefaultInputDeviceGUID, sizeof(string256), lpNewString);
                }
            }

            _RELEASE(pDeviceThis);
        }

        pCaptureEndPoints->GetCount(&CountOfInputs);
        pRenderEndPoints->GetCount(&CountOfOutputs);
    }
    else
    {
        return;
    }

    /*
        Process output information
    */
    for (size_t i = 0; i < CountOfOutputs; i++)
    {
        string256 lpNewString = {};
        LPWSTR lpString = nullptr;
        WASAPIDeviceDesc DeviceDescriptor = {};
        WAVEFORMATEX waveFormat = {};
        PROPVARIANT value = {};
        IMMDevice* pDevice = nullptr;
        IPropertyStore* pProperty = nullptr;
        IAudioClient* pAudioClient = nullptr;

        pRenderEndPoints->Item(i, &pDevice);

        if (pDevice)
        {
            DeviceDescriptor.DeviceFormat.DeviceId = i;

            pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
            pDevice->GetId(&lpString);

            // convert to UTF-8
            if (WideCharToMultiByte(CP_UTF8, 0, lpString, -1, lpNewString, 256, nullptr, nullptr))
            {
                xr_strcpy(DeviceDescriptor.DeviceGUID, sizeof(string256), lpNewString);
            }

            CoTaskMemFree(lpString);

            // get property store for format and device name
            if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProperty)))
            {
                PropVariantInit(&value);

                if (SUCCEEDED(pProperty->GetValue(PKEY_AudioEngine_DeviceFormat, &value)))
                {
                    CopyMemory(&waveFormat, value.blob.pBlobData,
                        std::min(sizeof(WAVEFORMATEX), (ULONGLONG)value.blob.cbSize));
                }

                PropVariantClear(&value);
                PropVariantInit(&value);

                if (SUCCEEDED(pProperty->GetValue(PKEY_Device_FriendlyName, &value)))
                {
                    if (value.vt == VT_LPWSTR)
                    {
                        // we need to get size of data to allocate
                        int StringSize =
                            WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, -1, nullptr, 0, nullptr, nullptr);
                        string256 lpNewString = {};

                        if (StringSize && StringSize < sizeof(string256))
                        {
                            // convert to UTF-8
                            if (WideCharToMultiByte(
                                    CP_UTF8, 0, value.pwszVal, -1, lpNewString, StringSize, nullptr, nullptr))
                            {
                                xr_strcpy(DeviceDescriptor.DeviceName, sizeof(string256), lpNewString);
                            }
                        }
                    }
                }

                PropVariantClear(&value);
                _RELEASE(pProperty);
            }

            DeviceDescriptor.DeviceFormat.IsCaptureDevice = false;
            DeviceDescriptor.DeviceFormat.Bits = waveFormat.wBitsPerSample;
            DeviceDescriptor.DeviceFormat.Channels = waveFormat.nChannels;
            DeviceDescriptor.DeviceFormat.SampleRate = waveFormat.nSamplesPerSec;

            // Get waveformat struct and check for float format
            WAVEFORMATEX* pWave = nullptr;
            if (SUCCEEDED(pAudioClient->GetMixFormat(&pWave)))
            {
                if (pWave->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
                {
                    if (pWave->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                    {
                        WAVEFORMATEXTENSIBLE* pTmp = (WAVEFORMATEXTENSIBLE*)pWave;
                        DeviceDescriptor.DeviceFormat.IsFloat = pTmp->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

                        if (DeviceDescriptor.DeviceFormat.IsFloat)
                        {
                            DeviceDescriptor.DeviceFormat.Bits = 32;
                        }
                    }
                }
                else
                {
                    DeviceDescriptor.DeviceFormat.IsFloat = true;
                }
            }

            DeviceDescriptor.DeviceFormat.Bits = pWave->wBitsPerSample;
            DeviceDescriptor.DeviceFormat.Channels = pWave->nChannels;
            DeviceDescriptor.DeviceFormat.SampleRate = pWave->nSamplesPerSec;

            if (pAudioClient)
            {
                UINT32 HostSize = 0;
                pAudioClient->GetBufferSize(&HostSize);
                DeviceDescriptor.DeviceFormat.FramesLatency = HostSize;
            }
        }

        _RELEASE(pAudioClient);
        _RELEASE(pDevice);

        OutputDevicesVector.push_back(DeviceDescriptor);
    }

    for (size_t i = 0; i < CountOfInputs; i++)
    {
        string256 lpNewString = {};
        LPWSTR lpString = nullptr;
        WASAPIDeviceDesc DeviceDescriptor = {};
        WAVEFORMATEX waveFormat = {};
        PROPVARIANT value = {};
        IMMDevice* pDevice = nullptr;
        IPropertyStore* pProperty = nullptr;
        IAudioClient* pAudioClient = nullptr;

        pCaptureEndPoints->Item(i, &pDevice);

        if (pDevice)
        {
            DeviceDescriptor.DeviceFormat.DeviceId = i;

            pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
            pDevice->GetId(&lpString);

            // convert to UTF-8
            if (WideCharToMultiByte(CP_UTF8, 0, lpString, -1, lpNewString, 256, nullptr, nullptr))
            {
                xr_strcpy(DeviceDescriptor.DeviceGUID, sizeof(string256), lpNewString);
            }

            CoTaskMemFree(lpString);

            // get property store for format and device name
            if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProperty)))
            {
                PropVariantInit(&value);

                if (SUCCEEDED(pProperty->GetValue(PKEY_AudioEngine_DeviceFormat, &value)))
                {
                    CopyMemory(&waveFormat, value.blob.pBlobData,
                        std::min(sizeof(WAVEFORMATEX), (ULONGLONG)value.blob.cbSize));
                }

                PropVariantClear(&value);
                PropVariantInit(&value);

                if (SUCCEEDED(pProperty->GetValue(PKEY_Device_FriendlyName, &value)))
                {
                    if (value.vt == VT_LPWSTR)
                    {
                        // we need to get size of data to allocate
                        int StringSize =
                            WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, -1, nullptr, 0, nullptr, nullptr);
                        string256 lpNewString = {};

                        if (StringSize && StringSize < sizeof(string256))
                        {
                            // convert to UTF-8
                            if (WideCharToMultiByte(
                                    CP_UTF8, 0, value.pwszVal, -1, lpNewString, StringSize, nullptr, nullptr))
                            {
                                xr_strcpy(DeviceDescriptor.DeviceName, sizeof(string256), lpNewString);
                            }
                        }
                    }
                }

                PropVariantClear(&value);
                _RELEASE(pProperty);
            }

            DeviceDescriptor.DeviceFormat.IsCaptureDevice = false;
            DeviceDescriptor.DeviceFormat.Bits = waveFormat.wBitsPerSample;
            DeviceDescriptor.DeviceFormat.Channels = waveFormat.nChannels;
            DeviceDescriptor.DeviceFormat.SampleRate = waveFormat.nSamplesPerSec;

            // Get waveformat struct and check for float format
            WAVEFORMATEX* pWave = nullptr;
            if (SUCCEEDED(pAudioClient->GetMixFormat(&pWave)))
            {
                if (pWave->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
                {
                    if (pWave->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                    {
                        WAVEFORMATEXTENSIBLE* pTmp = (WAVEFORMATEXTENSIBLE*)pWave;
                        DeviceDescriptor.DeviceFormat.IsFloat = pTmp->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

                        if (DeviceDescriptor.DeviceFormat.IsFloat)
                        {
                            DeviceDescriptor.DeviceFormat.Bits = 32;
                        }
                    }
                }
                else
                {
                    DeviceDescriptor.DeviceFormat.IsFloat = true;
                }
            }

            DeviceDescriptor.DeviceFormat.Bits = pWave->wBitsPerSample;
            DeviceDescriptor.DeviceFormat.Channels = pWave->nChannels;
            DeviceDescriptor.DeviceFormat.SampleRate = pWave->nSamplesPerSec;

            if (pAudioClient)
            {
                UINT32 HostSize = 0;
                pAudioClient->GetBufferSize(&HostSize);
                DeviceDescriptor.DeviceFormat.FramesLatency = HostSize;
            }
        }

        _RELEASE(pAudioClient);
        _RELEASE(pDevice);

        InputDevicesVector.push_back(DeviceDescriptor);
    }

    // make token
    u32 _cnt = GetOutNumDevices();
    snd_devices_token = xr_alloc<xr_token>(_cnt + 1);
    snd_devices_token[_cnt].id = -1;
    snd_devices_token[_cnt].name = nullptr;

    for (u32 i = 0; i < _cnt; ++i)
    {
        snd_devices_token[i].id = i;
        snd_devices_token[i].name = xr_strdup(StringFromUTF8(OutputDevicesVector[i].DeviceName, std::locale("")).c_str());
    }
    //--

    _RELEASE(pCaptureEndPoints);
    _RELEASE(pRenderEndPoints);
    _RELEASE(deviceEnumerator);
}

void CWASAPIDeviceEnumerate::GetInDeviceName(u32 DeviceIndex, string256& DeviceName) const
{
    memcpy(DeviceName, InputDevicesVector[DeviceIndex].DeviceName, sizeof(string256));
}

void CWASAPIDeviceEnumerate::GetOutDeviceName(u32 DeviceIndex, string256& DeviceName) const
{
    memcpy(DeviceName, OutputDevicesVector[DeviceIndex].DeviceName, sizeof(string256));
}
