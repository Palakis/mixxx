// encoderopus.cpp
// Create on August 15th 2017 by Palakis

#include "util/logger.h"

#include "encoder/encoderopus.h"

namespace {
// From libjitsi's Opus encoder:
// 1 byte TOC + maximum frame size (1275)
// See https://tools.ietf.org/html/rfc6716#section-3.2
const int kMaxOpusBufferSize = 1+1275;

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
    m_pOpusBuffer = (unsigned char*)malloc(kMaxOpusBufferSize);
    ogg_stream_init(&m_oggStream, rand());
}

EncoderOpus::~EncoderOpus() {
    if(m_pOpus) {
        opus_encoder_destroy(m_pOpus);
    }

    if(m_pFrameBuffer) {
        free(m_pFrameBuffer);
    }

    ogg_stream_clear(&m_oggStream);
    delete m_pOpusBuffer;
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

void EncoderOpus::initStream() {
    // Based on BUTT's Opus encoder implementation
    ogg_packet packet;
    ogg_stream_clear(&m_oggStream);
    ogg_stream_init(&m_oggStream, getSerial());
    m_granulePos = 0;
    m_packetNumber = 0;

    int headerSize = 0;
    unsigned char* headerData = createOpusHeader(&headerSize);

    int tagsSize = 0;
    unsigned char* tagsData = createOpusTags(&tagsSize);

    // Push stream header
    packet.b_o_s = 1;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = m_packetNumber++;
    packet.packet = headerData;
    packet.bytes = headerSize;

    ogg_stream_packetin(&m_oggStream, &packet);

    // Push tags
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = m_packetNumber++;
    packet.packet = tagsData;
    packet.bytes = tagsSize;

    ogg_stream_packetin(&m_oggStream, &packet);

    free(headerData);
    free(tagsData);
}

unsigned char* EncoderOpus::createOpusHeader(int* size) {
    unsigned char* data = (unsigned char*)malloc(1024);
    memset(data, 0, *size);
    int i = 0; // Current position

    // Opus identification header
    // Format from https://tools.ietf.org/html/rfc7845.html#section-5.1

    // Magic signature (8 bytes)
    memcpy(data + 0, "OpusHead", 8);
    i += 8;

    // Version number (1 byte, fixed to 1)
    data[i++] = 0x01;

    // Channel count (1 byte)
    data[i++] = (char)m_channels;

    // Pre-skip (2 bytes, little-endian)
    int preskip = 0;
    opus_encoder_ctl(m_pOpus, OPUS_GET_LOOKAHEAD(&preskip));
    for(int x = 0; x < 2; x++) {
        data[i++] = (preskip >> (x*8)) & 0xFF;
    }

    // Sample rate (4 bytes, little endian)
    for(int x = 0; x < 4; x++) {
        data[i++] = (m_samplerate >> (x*8)) & 0xFF;
    }

    // Output gain (2 bytes, little-endian, fixed to 0)
    data[i++] = 0;
    data[i++] = 0;

    // Channel mapping (1 byte, fixed to 0, means one stream)
    data[i++] = 0;

    // Ignore channel mapping table
    *size = i;
    return data;
}

unsigned char* EncoderOpus::createOpusTags(int* size) {
    unsigned char* data = (unsigned char*)malloc(1024);
    memset(data, 0, *size);
    int i = 0; // Current position

    // Opus comment header
    // Format from https://tools.ietf.org/html/rfc7845.html#section-5.2

    // Magic signature (8 bytes)
    memcpy(data + 0, "OpusTags", 8);
    i += 8;

    // Vendor string (mandatory)
    // length field (4 bytes, little-endian) + actual string
    const char* version = opus_get_version_string();
    int versionLength = strlen(version);
    // Write length field
    for(int x = 0; x < 4; x++) {
        data[i++] = (versionLength >> (x*8)) & 0xFF;
    }
    // Write string
    memcpy(data + i, version, versionLength);
    i += versionLength;

    // Number of comments
    data[i++] = 1;

    // First and only comment: encoder info
    // length field (4 bytes, little-endian) + actual string
    const char* encoderInfo = "ENCODER=mixxx/libopus";
    int infoLength = strlen(encoderInfo);
    // Write length field
    for(int x = 0; x < 4; x++) {
        data[i++] = (infoLength >> (x*8)) & 0xFF;
    }
    // Write string
    memcpy(data + i, encoderInfo, infoLength);
    i += infoLength;

    *size = i;
    return data;
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

        int samplesPerChannel = readRequired / m_channels;
        int result = opus_encode_float(m_pOpus, dataPtr, samplesPerChannel, m_pOpusBuffer, kMaxOpusBufferSize);
        free(dataPtr);

        if(result < 1) {
            kLogger.warning() << "opus_encode_float failed:" << opusErrorString(result);
            return;
        }

        unsigned char* packetData = (unsigned char*)malloc(result);
        memcpy(packetData, m_pOpusBuffer, result);

        ogg_packet packet;
        packet.b_o_s = 0;
        packet.e_o_s = 0;
        packet.granulepos = m_granulePos;
        packet.packetno = m_packetNumber;
        packet.packet = packetData;
        packet.bytes = result;

        m_granulePos += samplesPerChannel;
        m_packetNumber += 1;

        kLogger.debug() << "encoding success, pushing to stream";
        pushPacketToStream(&packet);
        free(packetData);
    }
}

void EncoderOpus::pushPacketToStream(ogg_packet* pPacket) {
    if(!pPacket) {
        return;
    }

    // Push headers prepared by initStream if not already done
    int result;
    if(m_header_write) {
        while (true) {
            result = ogg_stream_flush(&m_oggStream, &m_oggPage);
            if (result == 0)
                break;

            kLogger.debug() << "pushing headers to output";
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

        kLogger.debug() << "pushing data page to output";
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

