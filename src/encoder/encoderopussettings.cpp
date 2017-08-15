// encoderopussettings.cpp
// Create on August 15th 2017 by Palakis

#include "encoder/encoderopussettings.h"
#include "recording/defs_recording.h"
#include "util/logger.h"

namespace {
const int kDefaultBitrateIndex = 6;
const char* kQualityKey = "Opus_Quality";
const mixxx::Logger kLogger("EncoderOpusSettings");
}

EncoderOpusSettings::EncoderOpusSettings(UserSettingsPointer pConfig)
    : m_pConfig(pConfig) {
    m_qualList.append(32);
    m_qualList.append(48);
    m_qualList.append(64);
    m_qualList.append(80);
    m_qualList.append(96);
    m_qualList.append(112);
    m_qualList.append(128);
    m_qualList.append(160);
    m_qualList.append(192);
    m_qualList.append(224);
    m_qualList.append(256);
    m_qualList.append(320);
}

EncoderOpusSettings::~EncoderOpusSettings() {
}

QList<int> EncoderOpusSettings::getQualityValues() const {
    return m_qualList;
}

void EncoderOpusSettings::setQualityByValue(int qualityValue) {
    // Same behavior as Vorbis: Opus does not have a fixed set of
    // bitrates, so we can accept any value.
    int indexValue;
    if (m_qualList.contains(qualityValue)) {
        indexValue = m_qualList.indexOf(qualityValue);
    } else {
        // If we let the user write a bitrate value, this would allow to save such value.
        indexValue = qualityValue;
    }

    m_pConfig->setValue<int>(ConfigKey(RECORDING_PREF_KEY, kQualityKey), indexValue);
}

void EncoderOpusSettings::setQualityByIndex(int qualityIndex) {
    if (qualityIndex >= 0 && qualityIndex < m_qualList.size()) {
        m_pConfig->setValue<int>(ConfigKey(RECORDING_PREF_KEY, kQualityKey), qualityIndex);
    } else {
        kLogger.warning() << "Invalid qualityIndex given to EncoderVorbisSettings: "
                          << qualityIndex << ". Ignoring it";
    }
}
int EncoderOpusSettings::getQuality() const {
    int qualityIndex = m_pConfig->getValue(
            ConfigKey(RECORDING_PREF_KEY, kQualityKey), kDefaultBitrateIndex);

    if (qualityIndex >= 0 && qualityIndex < m_qualList.size()) {
        return m_qualList.at(qualityIndex);
    } else {
        // Same as Vorbis: Opus does not have a fixed set of
        // bitrates, so we can accept any value.
        return qualityIndex;
    }
}

int EncoderOpusSettings::getQualityIndex() const {
    int qualityIndex = m_pConfig->getValue(
            ConfigKey(RECORDING_PREF_KEY, kQualityKey), kDefaultBitrateIndex);

    if (qualityIndex >= 0 && qualityIndex < m_qualList.size()) {
        return qualityIndex;
    } else {
        kLogger.warning() << "Invalid qualityIndex in EncoderVorbisSettings "
                          << qualityIndex << "(Max is:"
                          << m_qualList.size() << ") . Ignoring it and"
                          << "returning default which is" << kDefaultBitrateIndex;
        return kDefaultBitrateIndex;
    }
}
