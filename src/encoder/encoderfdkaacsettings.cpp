// encoderfdkaacsettings.cpp
// Created on Aug 15 2017 by Palakis

#include "recording/defs_recording.h"
#include "util/logger.h"

#include "encoder/encoderfdkaacsettings.h"

namespace {
const int kDefaultQualityIndex = 6;
const char* kQualityKey = "FdkAac_Quality";
const mixxx::Logger kLogger("EncoderFdkAacSettings");
}

EncoderFdkAacSettings::EncoderFdkAacSettings(
        UserSettingsPointer pConfig,
        QString format)
        : m_pConfig(pConfig),
          m_format(std::move(format)) {
    m_qualList.append(32);
    m_qualList.append(48);
    m_qualList.append(64);
    m_qualList.append(80);
    m_qualList.append(96);
    m_qualList.append(112); // stereo
    m_qualList.append(128); // stereo
    m_qualList.append(160); // stereo
    m_qualList.append(192); // stereo
    m_qualList.append(224); // stereo
    m_qualList.append(256); // stereo
    m_qualList.append(320); // stereo
}

EncoderFdkAacSettings::~EncoderFdkAacSettings() {
}

QList<int> EncoderFdkAacSettings::getQualityValues() const {
    return m_qualList;
}

int EncoderFdkAacSettings::getQuality() const {
    return m_qualList.at(getQualityIndex());
}

int EncoderFdkAacSettings::getQualityIndex() const {
    int qualityIndex = m_pConfig->getValue(
            ConfigKey(RECORDING_PREF_KEY, kQualityKey), kDefaultQualityIndex);
    if (qualityIndex >= 0 && qualityIndex < m_qualList.size()) {
        return qualityIndex;
    } else {
        kLogger.warning()
                << "Invalid qualityIndex:"
                << qualityIndex << "(Max is:" << m_qualList.size() << "). Ignoring it"
                << "and returning default, which is:" << kDefaultQualityIndex;
    }
    return kDefaultQualityIndex;
}
