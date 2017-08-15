// encoderopus.h
// Create on August 15th 2017 by Palakis

#ifndef ENCODER_ENCODEROPUS_H
#define ENCODER_ENCODEROPUS_H

#include "encoder/encoder.h"
#include "encoder/encodercallback.h"

class EncoderOpus: public Encoder {
  public:
    EncoderOpus(EncoderCallback* pCallback = nullptr);
    virtual ~EncoderOpus();

    int initEncoder(int samplerate, QString errorMessage) override;
    void encodeBuffer(const CSAMPLE *samples, const int size) override;
    void updateMetaData(const QString& artist, const QString& title, const QString& album) override;
    void flush() override;
    void setEncoderSettings(const EncoderSettings& settings) override;

  private:
    EncoderCallback* m_pCallback;
};

#endif // ENCODER_ENCODEROPUS_H
