// encoderopus.cpp
// Create on August 15th 2017 by Palakis

#include "util/logger.h"

#include "encoder/encoderopus.h"

namespace {
const int kSamplesPerFrame = 1920; // Opus' accepted sample size for 48khz
const mixxx::Logger kLogger("EncoderOpus");
}

EncoderOpus::EncoderOpus(EncoderCallback* pCallback)
    : m_bitrate(0),
      m_channels(0),
      m_samplerate(0),
      m_pCallback(pCallback),
      m_pOpus(nullptr),
      m_pFrameBuffer(nullptr),
      m_header_write(false) {
}

EncoderOpus::~EncoderOpus() {
    if(m_pOpus) {
        opus_encoder_destroy(m_pOpus);
    }

    if(m_pFrameBuffer) {
        free(m_pFrameBuffer);
    }
}

void EncoderOpus::setEncoderSettings(const EncoderSettings& settings) {
    m_bitrate = settings.getQuality();
    switch(settings.getChannelMode()) {
        case EncoderSettings::ChannelMode::MONO:
            m_channels = 1;
            break;
        case EncoderSettings::ChannelMode::STEREO:
            m_channels = 2;
            break;
        case EncoderSettings::ChannelMode::AUTOMATIC:
            m_channels = 2;
            break;
    }
}

int EncoderOpus::initEncoder(int samplerate, QString errorMessage) {
    (void)errorMessage;
    int result;

    m_samplerate = samplerate;
    m_pOpus = opus_encoder_create(m_samplerate, m_channels, OPUS_APPLICATION_AUDIO, &result);

    if(result != OPUS_OK) {
        kLogger.warning() << "opus_encoder_create failed:" << opusErrorString(result);
        return -1;
    }

    // Twice the required sample count
    m_pFrameBuffer = new FIFO<CSAMPLE>((kSamplesPerFrame * m_channels) * 2);

    return 0;
}

void EncoderOpus::encodeBuffer(const CSAMPLE *samples, const int size) {
    if(!m_pOpus) {
        return;
    }

    int writeCount = math_min(size, m_pFrameBuffer->writeAvailable());
    if(writeCount > 0) {
        m_pFrameBuffer->write(samples, writeCount);
    }

    if(m_pFrameBuffer->readAvailable() >= kSamplesPerFrame) {
        int dataSize = kSamplesPerFrame / m_channels;
        CSAMPLE* dataPtr;
        m_pFrameBuffer->read(dataPtr, dataSize);

        // Based off libjitsi's Opus encoder:
        // 1 byte TOC + maximum frame size (1275)
        // See https://tools.ietf.org/html/rfc6716#section-3.2
        int maxSize = 1+1275;
        unsigned char* tmpPacket = (unsigned char*)malloc(maxSize);

        int result = opus_encode_float(m_pOpus, dataPtr, dataSize, tmpPacket, maxSize);
        if(result < 1) {
            kLogger.warning() << "opus_encode_float failed:" << opusErrorString(result);
            free((unsigned char*)tmpPacket);
            return;
        }

        unsigned char* packet = (unsigned char*)malloc(result);
        memcpy(packet, tmpPacket, result);
        free(tmpPacket);

        // TODO: Mux Opus datastream into an Ogg bitstream
        m_pCallback->write(nullptr, packet, 0, result);
    }
}

void EncoderOpus::updateMetaData(const QString& artist, const QString& title, const QString& album) {
    (void)artist, (void)title, (void)album;
}

void EncoderOpus::flush() {
}

QString EncoderOpus::opusErrorString(int error) {
    switch(error) {
        case OPUS_OK:
            return "OPUS_OK";
        case OPUS_BAD_ARG:
            return "OPUS_BAD_ARG";
        case OPUS_BUFFER_TOO_SMALL:
            return "OPUS_BUFFER_TOO_SMALL";
        case OPUS_INTERNAL_ERROR:
            return "OPUS_INTERNAL_ERROR";
        case OPUS_INVALID_PACKET:
            return "OPUS_INVALID_PACKET";
        case OPUS_UNIMPLEMENTED:
            return "OPUS_UNIMPLEMENTED";
        case OPUS_INVALID_STATE:
            return "OPUS_INVALID_STATE";
        case OPUS_ALLOC_FAIL:
            return "OPUS_ALLOC_FAIL";
        default:
            return "Unknown error";
    }
}

