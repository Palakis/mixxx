// encoderopus.h
// Create on August 15th 2017 by Palakis

#ifndef ENCODER_ENCODEROPUS_H
#define ENCODER_ENCODEROPUS_H

#include <QMap>
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
    static int getSerial();
    void initStream();
    void pushHeaderPacket();
    void pushTagsPacket();
    void writePage(ogg_packet* pPacket);
    void processFIFO();

    int m_bitrate;
    int m_bitrateMode;
    int m_channels;
    int m_samplerate;
    EncoderCallback* m_pCallback;
    FIFO<CSAMPLE>* m_pFifoBuffer;
    CSAMPLE* m_pFifoChunkBuffer;
    OpusEncoder* m_pOpus;
    unsigned char* m_pOpusDataBuffer;
    ogg_stream_state m_oggStream;
    ogg_page m_oggPage;
    bool m_header_write;
    int m_packetNumber;
    ogg_int64_t m_granulePos;
    QMap<QString, QString> m_opusComments;
};

#endif // ENCODER_ENCODEROPUS_H
