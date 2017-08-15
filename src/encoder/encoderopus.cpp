// encoderopus.cpp
// Create on August 15th 2017 by Palakis

#include "util/logger.h"

#include "encoder/encoderopus.h"

namespace {
typedef struct {
    int version;
    int channels;
    int preskip;
    ogg_uint32_t input_sample_rate;
    int gain;
    int channel_mapping;
    int nb_streams;
    int nb_coupled;
    unsigned char stream_map[255];
} OpusHeader;

const int kChannelSamplesPerFrame = 1920; // Opus' accepted sample size for 48khz
const mixxx::Logger kLogger("EncoderOpus");
}

EncoderOpus::EncoderOpus(EncoderCallback* pCallback)
    : m_bitrate(0),
      m_channels(0),
      m_samplerate(0),
      m_pCallback(pCallback),
      m_pFrameBuffer(nullptr),
      m_pOpus(nullptr),
      m_header_write(false),
      m_packetNumber(0),
      m_granulePos(0) {
    ogg_stream_init(&m_oggStream, getSerial());
}

EncoderOpus::~EncoderOpus() {
    if(m_pOpus) {
        opus_encoder_destroy(m_pOpus);
    }

    if(m_pFrameBuffer) {
        free(m_pFrameBuffer);
    }

    ogg_stream_clear(&m_oggStream);
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

    // Optimize encoding for high-quality music
    opus_encoder_ctl(m_pOpus, OPUS_SET_COMPLEXITY(10));
    opus_encoder_ctl(m_pOpus, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

    // Four times the required sample count to give enough room for buffering
    m_pFrameBuffer = new FIFO<CSAMPLE>(m_channels * kChannelSamplesPerFrame * 4);

    initStream();
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

    int readRequired = kChannelSamplesPerFrame * 2;
    if(m_pFrameBuffer->readAvailable() >= readRequired) {
        CSAMPLE* dataPtr = (CSAMPLE*)malloc(readRequired * sizeof(CSAMPLE));
        m_pFrameBuffer->read(dataPtr, readRequired);

        // Based off libjitsi's Opus encoder:
        // 1 byte TOC + maximum frame size (1275)
        // See https://tools.ietf.org/html/rfc6716#section-3.2
        int maxSize = 1+1275;
        unsigned char* tmpPacket = (unsigned char*)malloc(maxSize);

        int samplesPerChannel = readRequired / m_channels;
        int result = opus_encode_float(m_pOpus, dataPtr, samplesPerChannel, tmpPacket, maxSize);
        free(dataPtr);

        if(result < 1) {
            kLogger.warning() << "opus_encode_float failed:" << opusErrorString(result);
            free(tmpPacket);
            return;
        }

        unsigned char* packetData = (unsigned char*)malloc(result);
        memcpy(packetData, tmpPacket, result);
        free(tmpPacket);

        ogg_packet packet;
        packet.b_o_s = 0;
        packet.e_o_s = 0;
        packet.granulepos = m_granulePos;
        packet.packetno = m_packetNumber;
        packet.packet = packetData;
        packet.bytes = result;

        m_granulePos += samplesPerChannel;
        m_packetNumber += 1;

        pushPacketToStream(&packet);
        free(packetData);
    }
}

void EncoderOpus::initStream() {
    // Based on BUTT's Opus encoder implementation
    ogg_packet packet;
    ogg_stream_clear(&m_oggStream);
    ogg_stream_init(&m_oggStream, getSerial());
    m_granulePos = 0;
    m_packetNumber = 0;

    // Push Opus header
    packet.b_o_s = 1;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = m_packetNumber++;
    packet.packet = nullptr;
    packet.bytes = 0;

    ogg_stream_packetin(&m_oggStream, &packet);

    /*
    // Push tags
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = m_packetNumber++;
    packet.packet = nullptr;
    packet.bytes = 0;

    ogg_stream_packetin(&m_oggStream, &packet);
    */
}

void EncoderOpus::pushPacketToStream(ogg_packet* pPacket) {
    if(!pPacket) {
        return;
    }

    // Write initial stream header if not already done
    int result;
    if(m_header_write) {
        while (true) {
            result = ogg_stream_flush(&m_oggStream, &m_oggPage);
            if (result == 0)
                break;

            m_pCallback->write(m_oggPage.header, m_oggPage.body,
                               m_oggPage.header_len, m_oggPage.body_len);
        }
        m_header_write = false;
    }

    // Push Opus Ogg packets to the stream
    ogg_stream_packetin(&m_oggStream, pPacket);

    // Try to send available Ogg pages to the output
    bool eos = false;
    while (!eos) {
        int result = ogg_stream_pageout(&m_oggStream, &m_oggPage);
        if (result == 0) {
            break;
        }

        m_pCallback->write(m_oggPage.header, m_oggPage.body,
                           m_oggPage.header_len, m_oggPage.body_len);

        if (ogg_page_eos(&m_oggPage)) {
            eos = true;
        }
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

int EncoderOpus::getSerial()
{
    static int prevSerial = 0;
    int serial = rand();
    while (prevSerial == serial)
        serial = rand();
    prevSerial = serial;
    kLogger.debug() << "RETURNING SERIAL " << serial;
    return serial;
}

