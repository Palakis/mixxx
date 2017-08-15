// encoderopussettings.h
// Create on August 15th 2017 by Palakis

#ifndef ENCODER_ENCODEROPUSSETTINGS_H
#define ENCODER_ENCODEROPUSSETTINGS_H

#include "encoder/encodersettings.h"
#include "encoder/encoder.h"

class EncoderOpusSettings: public EncoderSettings {
  public:
    EncoderOpusSettings(UserSettingsPointer pConfig);
    virtual ~EncoderOpusSettings();

    // Indicates that it uses the quality slider section of the preferences
    bool usesQualitySlider() const override {
        return true;
    }
    // Indicates that it uses the compression slider section of the preferences
    bool usesCompressionSlider() const override {
        return false;
    }
    // Indicates that it uses the radio button section of the preferences.
    bool usesOptionGroups() const override {
        return false;
    }

    // Returns the list of quality values that it supports, to assign them to the slider
    QList<int> getQualityValues() const override;
    // Sets the quality value by its value
    void setQualityByValue(int qualityValue) override;
    // Sets the quality value by its index
    void setQualityByIndex(int qualityIndex) override;
    // Returns the current quality value
    int getQuality() const override;
    int getQualityIndex() const override;

  private:
    UserSettingsPointer m_pConfig;
    QList<int> m_qualList;
};

#endif // ENCODER_ENCODEROPUSSETTINGS_H
