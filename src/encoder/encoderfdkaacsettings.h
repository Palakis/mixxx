// encoderfdkaacsettings.h
// Created on Aug 15 2017 by Palakis

#ifndef ENCODER_ENCODERFDKAACSETTINGS_H
#define ENCODER_ENCODERFDKAACSETTINGS_H

#include <QList>

#include "encoder/encodersettings.h"
#include "encoder/encoder.h"

class EncoderFdkAacSettings : public EncoderRecordingSettings {
  public:
    EncoderFdkAacSettings(UserSettingsPointer pConfig, QString format);
    virtual ~EncoderFdkAacSettings();

    // Returns the list of quality values that it supports, to assign them to the slider
    QList<int> getQualityValues() const override;
    // Returns the current quality value
    int getQuality() const override;
    int getQualityIndex() const override;

    // Returns the format of this encoder settings.
    QString getFormat() const override {
        return ENCODING_OPUS;
    }

  private:
    QList<int> m_qualList;
    UserSettingsPointer m_pConfig;
    QString m_format;
};

#endif // ENCODER_ENCODERFDKAACSETTINGS_H
