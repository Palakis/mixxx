// encoderopussettings.cpp
// Create on August 15th 2017 by Palakis

#include "encoder/encoderopussettings.h"

EncoderOpusSettings::EncoderOpusSettings(UserSettingsPointer pConfig)
    : m_pConfig(pConfig) {
}

EncoderOpusSettings::~EncoderOpusSettings() {
}

QList<int> EncoderOpusSettings::getQualityValues() const {
    return QList<int>();
}

QList<int> EncoderOpusSettings::getVBRQualityValues() const {
    return QList<int>();
}

void EncoderOpusSettings::setQualityByValue(int qualityValue) {

}

void EncoderOpusSettings::setQualityByIndex(int qualityIndex) {

}

int EncoderOpusSettings::getQuality() const {
    return 0;
}

int EncoderOpusSettings::getQualityIndex() const {
    return -1;
}

QList<EncoderSettings::OptionsGroup> EncoderOpusSettings::getOptionGroups() const {
    return QList<EncoderSettings::OptionsGroup>();
}

void EncoderOpusSettings::setGroupOption(QString groupCode, int optionIndex) {

}

int EncoderOpusSettings::getSelectedOption(QString groupCode) const {
    return -1;
}

