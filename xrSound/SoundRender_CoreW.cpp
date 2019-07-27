#include "stdafx.h"
#include "WASAPIMixer.h"
#include "../xrCore/Text/StringConversion.hpp"

CSoundRender_CoreW* SoundRenderW = nullptr;

void CSoundRender_CoreW::update_listener(const Fvector& P, const Fvector& D, const Fvector& N, float dt)
{
    inherited::update_listener(P, D, N, dt);

    if (!Listener.position.similar(P))
    {
        Listener.position.set(P);
        bListenerMoved = TRUE;
    }
    Listener.orientation[0].set(D.x, D.y, -D.z);
    Listener.orientation[1].set(N.x, N.y, -N.z);
}

CSoundRender_CoreW::CSoundRender_CoreW()
{
    pInputDevice = nullptr;
    pOutputDevice = nullptr;

    pInputClient = nullptr;
    pOutputClient = nullptr;
    pEnumerator = nullptr;
}

CSoundRender_CoreW::~CSoundRender_CoreW()
{
    _clear();
}

void CSoundRender_CoreW::_initialize()
{
    IMMDeviceEnumerator* deviceEnumerator = NULL;
    IMMDeviceCollection* pEndPoints = NULL;
    IPropertyStore* pProperty = NULL;
    string256 szInDeviceId = {};
    string256 szOutDeviceId = {};
    WAVEFORMATEX waveFormat = {};
    PROPVARIANT value = {};

    if (!pEnumerator)
    {
        pEnumerator = new CWASAPIDeviceEnumerate();
    }
    pEnumerator->Enumerate();

    if (!pEnumerator->GetOutNumDevices())
    {
        xr_delete(pEnumerator);
        return;
    }

    if (!pEnumerator->GetInNumDevices())
    {
        IsInputSupported = false;
    }

    CopyMemory(szInDeviceId, pEnumerator->DefaultInputDeviceGUID, sizeof(string256));
    CopyMemory(szOutDeviceId, pEnumerator->DefaultOutputDeviceGUID, sizeof(string256));

    /*
        Create device enumerator for get input and output device from GUID string
    */
    if (SUCCEEDED(CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&deviceEnumerator))))
    {
        WCHAR szString[256] = {};

        /*
            Convert string to UTF16 and try to open device by GUID String
        */
        if (MultiByteToWideChar(CP_UTF8, 0, szInDeviceId, (int)strlen(szInDeviceId), szString, 256))
        {
            if (FAILED(deviceEnumerator->GetDevice(szString, &pInputDevice)))
            {
                if (SUCCEEDED(deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pInputDevice)))
                {
                    /*
                        It's not critical, so we put flag "-no_mic" into class var
                    */
                    IsInputSupported = true;
                }
                else
                {
                    Msg("! SOUND: WASAPI: Can't find capture device ");
                }
            }
            else
            {
                IsInputSupported = true;
            }
        }

        if (IsInputSupported)
        {
            CopyMemory(InputDesc.DeviceGUID, szInDeviceId, sizeof(string256));
        }

        /*
            Prepare to output device initialize
        */
        ZeroMemory(szString, sizeof(szString));
        if (MultiByteToWideChar(CP_UTF8, 0, szOutDeviceId, (int)strlen(szOutDeviceId), szString, 256))
        {
            if (FAILED(deviceEnumerator->GetDevice(szString, &pOutputDevice)))
            {
                if (FAILED(deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pOutputDevice)))
                {
                    /*
                        If we can't get default output device - just exit from initialize
                    */
                    Msg("! SOUND: WASAPI: Can't find render device ");
                    _RELEASE(pInputDevice);
                    _RELEASE(pOutputDevice);
                    _RELEASE(pInputClient);
                    _RELEASE(pOutputClient);
                    _RELEASE(deviceEnumerator);

                    xr_delete(pEnumerator);
                    return;
                }
            }
            
            CopyMemory(OutputDesc.DeviceGUID, szOutDeviceId, sizeof(string256));
        }

        if (IsInputSupported)
        {
            if (FAILED(pInputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pInputClient)))
            {
                Msg("! SOUND: WASAPI: Can't activate capture device ");
                IsInputSupported = false;
            }
        }

        if (FAILED(pOutputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pOutputClient)))
        {
            /*
                If we can't activate output device - just exit from initialize
            */
            Msg("! SOUND: WASAPI: Can't activate output device ");
            _RELEASE(pInputDevice);
            _RELEASE(pOutputDevice);
            _RELEASE(pInputClient);
            _RELEASE(pOutputClient);
            _RELEASE(deviceEnumerator);

            xr_delete(pEnumerator);
            return;
        }

        /*
            Open property store for input device
        */
        if (IsInputSupported)
        {
            if (SUCCEEDED(pInputDevice->OpenPropertyStore(STGM_READ, &pProperty)))
            {
                PropVariantInit(&value);

                if (SUCCEEDED(pProperty->GetValue(PKEY_AudioEngine_DeviceFormat, &value)))
                {
                    CopyMemory(&waveFormat, value.blob.pBlobData,
                        std::min(sizeof(WAVEFORMATEX), (value.blob.cbSize ? value.blob.cbSize : sizeof(WAVEFORMAT))));
                }

                PropVariantClear(&value);
                PropVariantInit(&value);

                /*
                    Get friendly device name
                */
                if (SUCCEEDED(pProperty->GetValue(PKEY_Device_FriendlyName, &value)))
                {
                    if (value.vt == VT_LPWSTR)
                    {
                        int StringSize = WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, -1, NULL, 0, NULL, NULL);
                        char lpNewString[260];
                        ZeroMemory(lpNewString, sizeof(lpNewString));

                        if (StringSize && StringSize < 256)
                        {
                            if (WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, -1, lpNewString, 260, NULL, NULL))
                            {
                                xr_strcpy(InputDesc.DeviceName, 256, lpNewString);
                                Msg("! SOUND: WASAPI: Current capture device name: %s",
                                    StringFromUTF8(lpNewString, std::locale("")));
                            }
                        }
                    }
                }

                PropVariantClear(&value);
                _RELEASE(pProperty);

                InputDesc.DeviceFormat.Bits = (BYTE)waveFormat.wBitsPerSample;
                InputDesc.DeviceFormat.Channels = (BYTE)waveFormat.nChannels;
                InputDesc.DeviceFormat.SampleRate = waveFormat.nSamplesPerSec;
                InputDesc.DeviceFormat.IsFloat = true;
            }

            /*
                Get waveformat struct and check for float format
            */
            WAVEFORMATEX* pInWave = nullptr;
            if (SUCCEEDED(pInputClient->GetMixFormat(&pInWave)))
            {
                if (pInWave->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
                {
                    if (pInWave->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                    {
                        WAVEFORMATEXTENSIBLE* pTmp = (WAVEFORMATEXTENSIBLE*)pInWave;
                        InputDesc.DeviceFormat.IsFloat = pTmp->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

                        if (OutputDesc.DeviceFormat.IsFloat)
                        {
                            InputDesc.DeviceFormat.Bits = 32;
                        }
                    }
                }
                else
                {
                    InputDesc.DeviceFormat.IsFloat = true;
                }
            }
        }

        ZeroMemory(&waveFormat, sizeof(WAVEFORMATEX));

        if (SUCCEEDED(pOutputDevice->OpenPropertyStore(STGM_READ, &pProperty)))
        {
            PropVariantInit(&value);

            if (SUCCEEDED(pProperty->GetValue(PKEY_AudioEngine_DeviceFormat, &value)))
            {
                CopyMemory(&waveFormat, value.blob.pBlobData,
                    std::min(sizeof(WAVEFORMATEX), (value.blob.cbSize ? value.blob.cbSize : sizeof(WAVEFORMAT))));
            }

            PropVariantClear(&value);
            PropVariantInit(&value);

            /*
                Get friendly device name
            */
            if (SUCCEEDED(pProperty->GetValue(PKEY_Device_FriendlyName, &value)))
            {
                if (value.vt == VT_LPWSTR)
                {
                    int StringSize = WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, -1, NULL, 0, NULL, NULL);
                    char lpNewString[260];
                    ZeroMemory(lpNewString, sizeof(lpNewString));

                    if (StringSize && StringSize < 256)
                    {
                        if (WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, -1, lpNewString, 260, NULL, NULL))
                        {
                            xr_strcpy(OutputDesc.DeviceName, 256, lpNewString);
                            Msg("! SOUND: WASAPI: Current render device name: %s",
                                StringFromUTF8(lpNewString, std::locale("")));

                             snd_devices_token[pEnumerator->GetOutNumDevices()].name =
                                xr_strdup(StringFromUTF8(OutputDesc.DeviceName, std::locale("")).c_str());
                        }
                    }
                }
            }

            PropVariantClear(&value);
            _RELEASE(pProperty);

            OutputDesc.DeviceFormat.Bits = (BYTE)waveFormat.wBitsPerSample;
            OutputDesc.DeviceFormat.Channels = (BYTE)waveFormat.nChannels;
            OutputDesc.DeviceFormat.SampleRate = waveFormat.nSamplesPerSec;
            OutputDesc.DeviceFormat.IsFloat = true;
        }

        /*
            Get waveformat struct and check for float format
        */
        WAVEFORMATEX* pWave = nullptr;
        if (SUCCEEDED(pOutputClient->GetMixFormat(&pWave)))
        {
            if (pWave->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
            {
                if (pWave->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                {
                    WAVEFORMATEXTENSIBLE* pTmp = (WAVEFORMATEXTENSIBLE*)pWave;
                    OutputDesc.DeviceFormat.IsFloat = pTmp->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

                    if (OutputDesc.DeviceFormat.IsFloat)
                    {
                        OutputDesc.DeviceFormat.Bits = 32;
                    }
                }
            }
            else
            {
                OutputDesc.DeviceFormat.IsFloat = true;
            }
        }
    }

    OutMixer.Initialize(this);

    inherited::_initialize();

    // Pre-create targets
    CSoundRender_TargetW* T = nullptr;
    for (u32 tit = 0; tit < u32(psSoundTargets); tit++)
    {
        T = new CSoundRender_TargetW();
        T->_parent(this);
        if (T->_initialize())
        {
            s_targets.push_back(T);
        }
        else
        {
            Log("! SOUND: WASAPI: Max targets - ", tit);
            T->_destroy();
            xr_delete(T);
            break;
        }
    }
}

void CSoundRender_CoreW::_clear()
{
    inherited::_clear();

    /*
        Release sound targets
    */
    CSoundRender_Target* T = nullptr;

    if (!s_targets.empty())
    {
        for (u32 tit = 0; tit < s_targets.size(); tit++)
        {
            T = s_targets[tit];
            T->_destroy();
            xr_delete(T);
        }
    }

    /*
        Release all our devices
    */
    _RELEASE(pInputDevice);
    _RELEASE(pOutputDevice);
    _RELEASE(pInputClient);
    _RELEASE(pOutputClient);

    xr_delete(pEnumerator);
}

void CSoundRender_CoreW::_restart() { inherited::_restart(); }
void CSoundRender_CoreW::set_master_volume(float f) { fMasterVolume = f; }
