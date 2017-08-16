// encoderfdkaac.h
// Created on Aug 15 2017 by Palakis

#ifndef ENCODER_ENCODERFDKAAC_H
#define ENCODER_ENCODERFDKAAC_H

#include <fdk-aac/aacenc_lib.h>
#include <QLibrary>

#include "encoder/encoder.h"
#include "util/fifo.h"

class EncoderFdkAac: public Encoder {
  public:
    EncoderFdkAac(EncoderCallback* pCallback, const char* pFormat);
    virtual ~EncoderFdkAac();

    int initEncoder(int samplerate, QString errorMessage) override;
    void encodeBuffer(const CSAMPLE *samples, const int sampleCount) override;
    void updateMetaData(const QString& artist, const QString& title, const QString& album) override;
    void flush() override;
    void setEncoderSettings(const EncoderSettings& settings) override;

  private:
    int m_aacAot;
    int m_bitrate;
    int m_channels;
    int m_samplerate;
    EncoderCallback* m_pCallback;
    QLibrary* m_library;
    FIFO<CSAMPLE>* m_pInputBuffer;
    HANDLE_AACENCODER m_aacEnc;
    AACENC_InfoStruct m_aacInfo;
};

#endif // ENCODER_ENCODERFDKAAC_H
