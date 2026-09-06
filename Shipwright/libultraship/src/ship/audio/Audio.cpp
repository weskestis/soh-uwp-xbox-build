#include "ship/audio/Audio.h"

#ifdef __APPLE__
#include "ship/audio/CoreAudioAudioPlayer.h"
#endif

#include "ship/Context.h"
#include "ship/config/Config.h"
#include "ship/controller/controldeck/ControlDeck.h"

namespace Ship {

Audio::~Audio() {
    SPDLOG_TRACE("destruct audio");
}

void Audio::InitAudioPlayer() {
    switch (GetCurrentAudioBackend()) {
#if defined(_WIN32) && !defined(ZELDA3D_UWP)
        case AudioBackend::WASAPI:
            mAudioPlayer = std::make_shared<WasapiAudioPlayer>(this->mAudioSettings);
            break;
#endif
#ifdef __APPLE__
        case AudioBackend::COREAUDIO:
            mAudioPlayer = std::make_shared<CoreAudioAudioPlayer>(this->mAudioSettings);
            break;
#endif
        case AudioBackend::SDL:
            mAudioPlayer = std::make_shared<SDLAudioPlayer>(this->mAudioSettings);
            break;
        default:
            mAudioPlayer = std::make_shared<NullAudioPlayer>(this->mAudioSettings);
            break;
    }

    if (mAudioPlayer && !mAudioPlayer->Init()) {
        // Failed to initialize system audio player.
        // Fallback to Null if the native system player does not work.
        SetCurrentAudioBackend(AudioBackend::NUL);
    }
}

std::shared_ptr<Config> Audio::CurrentConfig() {
    return Context::GetRawInstance()->GetConfig();
}

void Audio::Init() {

    mAvailableAudioBackends = std::make_shared<std::vector<AudioBackend>>();
#if defined(_WIN32) && !defined(ZELDA3D_UWP)
    mAvailableAudioBackends->push_back(AudioBackend::WASAPI);
#endif
#ifdef __APPLE__
    mAvailableAudioBackends->push_back(AudioBackend::COREAUDIO);
#endif
    mAvailableAudioBackends->push_back(AudioBackend::SDL);
    mAvailableAudioBackends->push_back(AudioBackend::NUL);

    ApplySavedSettings();
}

void Audio::ApplySavedSettings() {
    SetCurrentAudioBackend(GetSavedAudioBackend());
    SetAudioChannels(GetSavedAudioChannelsSetting());
}

std::shared_ptr<AudioPlayer> Audio::GetAudioPlayer() {
    return mAudioPlayer;
}

AudioBackend Audio::GetCurrentAudioBackend() {
    return mAudioBackend;
}

AudioBackend Audio::GetSavedAudioBackend() {
    std::string backendName = CurrentConfig()->GetString("Window.AudioBackend");
    if (backendName == "wasapi") {
#ifdef ZELDA3D_UWP
        // The UWP wrapper owns the app-container audio device through its SDL/WinRT runtime.
        // Migrate desktop settings copied into the package instead of selecting an unavailable backend.
        CurrentConfig()->SetString("Window.AudioBackend", "sdl");
        CurrentConfig()->Save();
        return AudioBackend::SDL;
#else
        return AudioBackend::WASAPI;
#endif
    }

    // Migrate pulse player in config to sdl
    if (backendName == "pulse") {
        CurrentConfig()->SetString("Window.AudioBackend", "sdl");
        CurrentConfig()->Save();
        return AudioBackend::SDL;
    }

    if (backendName == "coreaudio") {
        return AudioBackend::COREAUDIO;
    }

    if (backendName == "sdl") {
        return AudioBackend::SDL;
    }

    if (backendName == "null") {
        return AudioBackend::NUL;
    }

    SPDLOG_TRACE("Could not find AudioBackend matching value from config file ({}). Returning default AudioBackend.",
                 backendName);
#if defined(_WIN32) && !defined(ZELDA3D_UWP)
    return AudioBackend::WASAPI;
#endif

#ifdef __APPLE__
    return AudioBackend::COREAUDIO;
#endif

    return AudioBackend::SDL;
}

void Audio::SetCurrentAudioBackend(AudioBackend backend) {
    mAudioBackend = backend;

    switch (backend) {
        case AudioBackend::WASAPI:
            CurrentConfig()->SetString("Window.AudioBackend", "wasapi");
            break;
        case AudioBackend::COREAUDIO:
            CurrentConfig()->SetString("Window.AudioBackend", "coreaudio");
            break;
        case AudioBackend::SDL:
            CurrentConfig()->SetString("Window.AudioBackend", "sdl");
            break;
        case AudioBackend::NUL:
            CurrentConfig()->SetString("Window.AudioBackend", "null");
            break;
        default:
            CurrentConfig()->SetString("Window.AudioBackend", "");
    }
    CurrentConfig()->Save();

    InitAudioPlayer();
}

std::shared_ptr<std::vector<AudioBackend>> Audio::GetAvailableAudioBackends() {
    return mAvailableAudioBackends;
}

void Audio::SetAudioChannels(AudioChannelsSetting channels) {
    if (mAudioSettings.ChannelSetting != channels) {
        mAudioSettings.ChannelSetting = channels;
        // Reinitialize the existing audio player with the new channel configuration
        if (mAudioPlayer) {
            mAudioPlayer->SetAudioChannels(channels);
        }
    }
}

AudioChannelsSetting Audio::GetAudioChannels() const {
    return mAudioSettings.ChannelSetting;
}

AudioChannelsSetting Audio::GetSavedAudioChannelsSetting() {
    int32_t channelsSetting =
        CurrentConfig()->GetInt("CVars." CVAR_AUDIO_CHANNELS_SETTING, static_cast<int32_t>(AudioChannelsSetting::audioMax));
    switch (channelsSetting) {
        case AudioChannelsSetting::audioMatrix51:
            return AudioChannelsSetting::audioMatrix51;
        case AudioChannelsSetting::audioRaw51:
            return AudioChannelsSetting::audioRaw51;
        case AudioChannelsSetting::audioStereo:
        case AudioChannelsSetting::audioMax:
        default:
            return AudioChannelsSetting::audioStereo;
    }
}

} // namespace Ship
