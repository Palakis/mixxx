// encoderopus.h
// Create on August 15th 2017 by Palakis

#ifndef ENCODER_ENCODEROPUS_H
#define ENCODER_ENCODEROPUS_H

#include <QString>

#include <ogg/ogg.h>
#include <opus/opus.h>

#include "encoder/encoder.h"
#include "encoder/encodercallback.h"
#include "util/fifo.h"
#include "util/sample.h"

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
    static QString opusErrorString(int error);

    int m_bitrate;
    int m_channels;
    int m_samplerate;
    EncoderCallback* m_pCallback;
    OpusEncoder* m_pOpus;
    FIFO<CSAMPLE>* m_pFrameBuffer;
    bool m_header_write;
};

#endif // ENCODER_ENCODEROPUS_H
