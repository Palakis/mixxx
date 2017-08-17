// encoderopus.cpp
// Create on August 15th 2017 by Palakis

#include <QByteArray>
#include <QMapIterator>

#include "util/logger.h"

#include "encoder/encoderopus.h"

namespace {
// From libjitsi's Opus encoder:
// 1 byte TOC + maximum frame size (1275)
// See https://tools.ietf.org/html/rfc6716#section-3.2
const int kMaxOpusBufferSize = 1+1275;

const int kChannelSamplesPerFrame = 1920; // Opus' accepted sample size for 48khz
const int kReadRequired = kChannelSamplesPerFrame * 2;
const mixxx::Logger kLogger("EncoderOpus");
}

EncoderOpus::EncoderOpus(EncoderCallback* pCallback)
    : m_bitrate(0),
      m_channels(0),
      m_samplerate(0),
      m_pCallback(pCallback),
      m_pFifoBuffer(nullptr),
      m_pFifoChunkBuffer(nullptr),
      m_pOpus(nullptr),
      m_header_write(false),
      m_packetNumber(0),
      m_granulePos(0) {
    m_opusComments.insert("ENCODER", "mixxx/libopus");
    m_pOpusDataBuffer = new unsigned char[kMaxOpusBufferSize]();
    ogg_stream_init(&m_oggStream, rand());
}

EncoderOpus::~EncoderOpus() {
    if (m_pOpus)
        opus_encoder_destroy(m_pOpus);

    if (m_pFifoChunkBuffer)
        delete m_pFifoChunkBuffer;

    if (m_pFifoBuffer)
        delete m_pFifoBuffer;

    ogg_stream_clear(&m_oggStream);
    delete m_pOpusDataBuffer;
}

void EncoderOpus::setEncoderSettings(const EncoderSettings& settings) {
    // TODO(Palakis): support VBR
    m_bitrate = settings.getQuality();
    switch (settings.getChannelMode()) {
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

    if (samplerate != 48000) {
        kLogger.warning() << "initEncoder failed: samplerate not supported by Opus";

        QString invalidSamplerate = QObject::tr(
                "Using Opus at samplerates other than 48 kHz "
                "is not supported by the Opus encoder. Please use "
                "48000 Hz in \"Sound Hardware\" preferences "
                "or switch to a different encoding.");

        ErrorDialogProperties* props = ErrorDialogHandler::instance()->newDialogProperties();
        props->setType(DLG_WARNING);
        props->setTitle(QObject::tr("Encoder"));
        props->setText(invalidSamplerate);
        props->setKey(invalidSamplerate);
        ErrorDialogHandler::instance()->requestErrorDialog(props);
        return -1;
    }

    m_samplerate = samplerate;
    m_pOpus = opus_encoder_create(m_samplerate, m_channels, OPUS_APPLICATION_AUDIO, &result);

    if (result != OPUS_OK) {
        kLogger.warning() << "opus_encoder_create failed:" << opusErrorString(result);
        return -1;
    }

    // Optimize encoding for high-quality music
    opus_encoder_ctl(m_pOpus, OPUS_SET_COMPLEXITY(10)); // Highest setting
    opus_encoder_ctl(m_pOpus, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

    // Set bitrate in bits per second
    // m_bitrate is in kilobits per second, conversion needed
    opus_encoder_ctl(m_pOpus, OPUS_SET_BITRATE(m_bitrate * 1000));

    // TODO(Palakis): use constant or have the engine provide that value
    m_pFifoBuffer = new FIFO<CSAMPLE>(57344 * 2);
    m_pFifoChunkBuffer = new CSAMPLE[kChannelSamplesPerFrame * 2 * sizeof(CSAMPLE)]();
    initStream();

    return 0;
}

void EncoderOpus::initStream() {
    ogg_stream_clear(&m_oggStream);
    ogg_stream_init(&m_oggStream, getSerial());
    m_header_write = true;
    m_granulePos = 0;
    m_packetNumber = 0;

    pushHeaderPacket();
    pushTagsPacket();
}

// Binary header construction is done manually to properly
// handle endianness of multi-byte number fields
void EncoderOpus::pushHeaderPacket() {
    // Opus identification header
    // Format from https://tools.ietf.org/html/rfc7845.html#section-5.1

    // Header buffer size:
    // - Magic signature: 8 bytes
    // - Version: 1 byte
    // - Channel count: 1 byte
    // - Pre-skip: 2 bytes
    // - Samplerate: 4 bytes
    // - Output Gain: 2 bytes
    // - Mapping family: 1 byte
    // - Channel mapping table: ignored
    // Total: 19 bytes
    const int frameSize = 19;
    unsigned char* data = new unsigned char[frameSize]();
    int pos = 0; // Current position

    // Magic signature (8 bytes)
    memcpy(data + pos, "OpusHead", 8);
    pos += 8;

    // Version number (1 byte, fixed to 1)
    data[pos++] = 0x01;

    // Channel count (1 byte)
    data[pos++] = (char)m_channels;

    // Pre-skip (2 bytes, little-endian)
    int preskip = 0;
    opus_encoder_ctl(m_pOpus, OPUS_GET_LOOKAHEAD(&preskip));
    for (int x = 0; x < 2; x++) {
        data[pos++] = (preskip >> (x*8)) & 0xFF;
    }

    // Sample rate (4 bytes, little endian)
    for (int x = 0; x < 4; x++) {
        data[pos++] = (m_samplerate >> (x*8)) & 0xFF;
    }

    // Output gain (2 bytes, little-endian, fixed to 0)
    data[pos++] = 0;
    data[pos++] = 0;

    // Channel mapping (1 byte, fixed to 0, means one stream)
    data[pos++] = 0;

    // Ignore channel mapping table

    // Push finished header to stream
    ogg_packet packet;
    packet.b_o_s = 1;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = m_packetNumber++;
    packet.packet = data;
    packet.bytes = frameSize;

    ogg_stream_packetin(&m_oggStream, &packet);
    delete data;
}

void EncoderOpus::pushTagsPacket() {
    // Opus comment header
    // Format from https://tools.ietf.org/html/rfc7845.html#section-5.2

    QByteArray combinedComments;
    int commentCount = 0;

    const char* vendorString = opus_get_version_string();
    int vendorStringLength = strlen(vendorString);

    // == Compute tags frame size ==
    // - Magic signature: 8 bytes
    // - Vendor string length: 4 bytes
    // - Vendor string: dynamic size
    // - Comment list length: 4 bytes
    int frameSize = 8 + 4 + vendorStringLength + 4;
    // - Comment list: dynamic size
    QMapIterator<QString, QString> iter(m_opusComments);
    while(iter.hasNext()) {
        iter.next();
        QString comment = iter.key() + "=" + iter.value();

        // Convert comment to raw UTF-8 data;
        const char* commentData = comment.toUtf8().constData();
        int commentDataLength = strlen(commentData);
        QByteArray commentBytes(commentData, commentDataLength);

        // One comment is:
        // - 4 bytes of string length
        // - string data

        // Add comment length field and data to comments "list"
        for (int x = 0; x < 4; x++) {
            unsigned char fieldValue = (commentDataLength >> (x*8)) & 0xFF;
            combinedComments.append(fieldValue);
        }
        combinedComments.append(commentBytes);

        // Don't forget to include this comment in the overall size calculation
        frameSize += (4 + strlen(comment.toUtf8().constData()));
        commentCount++;
    }

    // == Actual frame building ==
    unsigned char* data = new unsigned char[frameSize]();
    int pos = 0; // Current position

    // Magic signature (8 bytes)
    memcpy(data + pos, "OpusTags", 8);
    pos += 8;

    // Vendor string (mandatory)
    // length field (4 bytes, little-endian) + actual string
    // Write length field
    for (int x = 0; x < 4; x++) {
        data[pos++] = (vendorStringLength >> (x*8)) & 0xFF;
    }
    // Write string
    memcpy(data + pos, vendorString, vendorStringLength);
    pos += vendorStringLength;

    // Number of comments (4 bytes, little-endian)
    for (int x = 0; x < 4; x++) {
        data[pos++] = (commentCount >> (x*8)) & 0xFF;
    }

    // Comment list (dynamic size)
    int commentListLength = combinedComments.size();
    memcpy(data + pos, combinedComments.constData(), commentListLength);
    pos += commentListLength;

    // Push finished tags frame to stream
    ogg_packet packet;
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = m_packetNumber++;
    packet.packet = data;
    packet.bytes = frameSize;

    kLogger.debug() << data;

    ogg_stream_packetin(&m_oggStream, &packet);
    delete data;
}

void EncoderOpus::encodeBuffer(const CSAMPLE *samples, const int size) {
    if (!m_pOpus) {
        return;
    }

    int writeRequired = size;
    int writeAvailable = m_pFifoBuffer->writeAvailable();
    if (writeRequired > writeAvailable) {
        kLogger.warning() << "FIFO buffer too small, loosing samples!"
                          << "required:" << writeRequired
                          << "; available: " << writeAvailable;
    }

    int writeCount = math_min(writeRequired, writeAvailable);
    if (writeCount > 0) {
        m_pFifoBuffer->write(samples, writeCount);
    }

    processFIFO();
}

void EncoderOpus::processFIFO() {
    while (m_pFifoBuffer->readAvailable() >= kReadRequired) {
        memset(m_pFifoChunkBuffer, 0, kReadRequired * sizeof(CSAMPLE));
        m_pFifoBuffer->read(m_pFifoChunkBuffer, kReadRequired);

        int samplesPerChannel = kReadRequired / m_channels;
        int result = opus_encode_float(m_pOpus, m_pFifoChunkBuffer, samplesPerChannel,
                m_pOpusDataBuffer, kMaxOpusBufferSize);

        if (result < 1) {
            kLogger.warning() << "opus_encode_float failed:" << opusErrorString(result);
            return;
        }

        ogg_packet packet;
        packet.b_o_s = 0;
        packet.e_o_s = 0;
        packet.granulepos = m_granulePos;
        packet.packetno = m_packetNumber;
        packet.packet = m_pOpusDataBuffer;
        packet.bytes = result;

        m_granulePos += samplesPerChannel;
        m_packetNumber += 1;

        writePage(&packet);
    }
}

void EncoderOpus::writePage(ogg_packet* pPacket) {
    if (!pPacket) {
        return;
    }

    // Push headers prepared by initStream if not already done
    int result;
    if (m_header_write) {
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

        m_pCallback->write(m_oggPage.header, m_oggPage.body,
                           m_oggPage.header_len, m_oggPage.body_len);

        if (ogg_page_eos(&m_oggPage)) {
            eos = true;
        }
    }
}

void EncoderOpus::updateMetaData(const QString& artist, const QString& title, const QString& album) {
    m_opusComments.insert("ARTIST", artist);
    m_opusComments.insert("TITLE", title);
    m_opusComments.insert("ALBUM", album);
}

void EncoderOpus::flush() {
    // At this point there may still be samples in the FIFO buffer
    processFIFO();
}

QString EncoderOpus::opusErrorString(int error) {
    QString errorString = "";
    switch (error) {
        case OPUS_OK:
            errorString = "OPUS_OK";
            break;
        case OPUS_BAD_ARG:
            errorString = "OPUS_BAD_ARG";
            break;
        case OPUS_BUFFER_TOO_SMALL:
            errorString = "OPUS_BUFFER_TOO_SMALL";
            break;
        case OPUS_INTERNAL_ERROR:
            errorString = "OPUS_INTERNAL_ERROR";
            break;
        case OPUS_INVALID_PACKET:
            errorString = "OPUS_INVALID_PACKET";
            break;
        case OPUS_UNIMPLEMENTED:
            errorString = "OPUS_UNIMPLEMENTED";
            break;
        case OPUS_INVALID_STATE:
            errorString = "OPUS_INVALID_STATE";
            break;
        case OPUS_ALLOC_FAIL:
            errorString = "OPUS_ALLOC_FAIL";
            break;
        default:
            return "Unknown error";
    }
    return errorString + (QString(" (%1)").arg(error));
}

int EncoderOpus::getSerial() {
    static int prevSerial = 0;

    int serial;
    do {
        serial = rand();
    } while(prevSerial == serial);

    prevSerial = serial;
    kLogger.debug() << "RETURNING SERIAL " << serial;
    return serial;
}

