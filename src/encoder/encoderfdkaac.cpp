// encoderfdkaac.cpp
// Created on Aug 15 2017 by Palakis

#include <QString>
#include <QStringList>

#include "recording/defs_recording.h"
#include "util/logger.h"
#include "util/sample.h"

#include "encoder/encoderfdkaac.h"

namespace {
// recommended in encoder documentation, section 2.4.1
const int kOutBufferBits = 6144;
const mixxx::Logger kLogger("EncoderFdkAac");
}

EncoderFdkAac::EncoderFdkAac(EncoderCallback* pCallback, const char* pFormat)
    : m_aacAot(0),
      m_bitrate(0),
      m_channels(0),
      m_samplerate(0),
      m_pCallback(pCallback),
      m_library(nullptr) ,
      m_aacEnc(),
      m_aacInfo() {

    if (strcmp(pFormat, ENCODING_AAC) == 0) {
        // MPEG-4 AAC-LC
        m_aacAot = AOT_AAC_LC;
    }
    else if (strcmp(pFormat, ENCODING_HEAAC) == 0) {
        // MPEG-4 HE-AAC
        m_aacAot = AOT_SBR;
    }

    // Load shared library
    // Code import from encodermp3.cpp
    QStringList libnames;
    QString libname = "";
#ifdef __LINUX__
    libnames << "fdk-aac";
#elif __WINDOWS__
    libnames << "libfdkaac.dll";
    libnames << "libfdk-aac.dll";
#elif __APPLE__
    // TODO(Palakis): double-check macOS paths
    libnames << "/usr/local/lib/libfdk-aac.dylib";
    //Using MacPorts (former DarwinPorts) results in ...
    libnames << "/opt/local/lib/libfdk-aac.dylib";
#endif

    for (const auto& libname : libnames) {
        m_library = new QLibrary(libname, 0);
        if (m_library->load()) {
            kLogger.debug() << "Successfully loaded encoder library " << libname;
            break;
        } else {
            kLogger.warning() << "Failed to load " << libname << ", " << m_library->errorString();
        }
        delete m_library;
        m_library = nullptr;
    }

    if (!m_library || !m_library->isLoaded()) {
        ErrorDialogProperties* props = ErrorDialogHandler::instance()->newDialogProperties();
        props->setType(DLG_WARNING);
        props->setTitle(QObject::tr("Encoder"));

        // TODO(Palakis): write installation guide on Mixxx's wiki
        // and include link in message below
        QString missingCodec = QObject::tr("<html>Mixxx cannot record or stream in AAC "
                "or AAC+ without the FDK-AAC encoder. Due to licensing issues, we cannot include this with Mixxx. "
                "To record or stream in AAC or AAC+, you must download <b>libfdk-aac</b> "
                "and install it on your system.");

#ifdef __LINUX__
        missingCodec = missingCodec.arg("linux");
#elif __WINDOWS__
        missingCodec = missingCodec.arg("windows");
#elif __APPLE__
        missingCodec = missingCodec.arg("mac_osx");
#endif
        props->setText(missingCodec);
        props->setKey(missingCodec);
        ErrorDialogHandler::instance()->requestErrorDialog(props);
        return;
    }
}

EncoderFdkAac::~EncoderFdkAac() {
    aacEncClose(&m_aacEnc);
}

void EncoderFdkAac::setEncoderSettings(const EncoderSettings& settings) {
    // TODO(Palakis): support more bitrate configurations
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

int EncoderFdkAac::initEncoder(int samplerate, QString errorMessage) {
    (void)errorMessage;
    m_samplerate = samplerate;

    // This initializes the encoder handle but not the encoder itself.
    // Actual encoder init is done below.
    aacEncOpen(&m_aacEnc, 0, m_channels);

    // AAC Object Type: specifies "mode": AAC-LC, HE-AAC, HE-AACv2, DAB AAC, etc...
    aacEncoder_SetParam(m_aacEnc, AACENC_AOT, m_aacAot);

    // Input audio samplerate
    aacEncoder_SetParam(m_aacEnc, AACENC_SAMPLERATE, m_samplerate);
    // Input and output audio channel count
    aacEncoder_SetParam(m_aacEnc, AACENC_CHANNELMODE, m_channels);
    // Input audio channel order (fixed to 1 for traditional WAVE ordering: L, R, ...)
    aacEncoder_SetParam(m_aacEnc, AACENC_CHANNELORDER, 1);

    // Output bitrate in bytes per seconds
    aacEncoder_SetParam(m_aacEnc, AACENC_BITRATE, m_bitrate * 1000);
    // Transport type (2 = ADTS)
    aacEncoder_SetParam(m_aacEnc, AACENC_TRANSMUX, 2);

    // Actual encoder init, validates settings provided above
    int result = aacEncEncode(m_aacEnc, nullptr, nullptr, nullptr, nullptr);
    if(result != AACENC_OK) {
        kLogger.warning() << "aac encoder init failed! error code:" << result;
        return -1;
    }

    aacEncInfo(m_aacEnc, &m_aacInfo);
    return 0;
}

void EncoderFdkAac::encodeBuffer(const CSAMPLE *samples, const int sampleCount) {
    // fdk-aac only accept pointers for most buffer settings.
    // Declare settings here and point to them below.
    int inSampleSize = sizeof(SAMPLE);
    int inDataSize = sampleCount * inSampleSize;
    SAMPLE* inData = (SAMPLE*)malloc(inDataSize);
    // fdk-aac doesn't support float samples, so convert
    // to integers instead
    SampleUtil::convertFloat32ToS16(inData, samples, sampleCount);
    int inDataDescription = IN_AUDIO_DATA;

    int outElemSize = sizeof(unsigned char);
    int outDataSize = kOutBufferBits * m_channels * outElemSize;
    unsigned char* outData = (unsigned char*)malloc(outDataSize);
    int outDataDescription = OUT_BITSTREAM_DATA;

    // === Input Buffer ===
    AACENC_BufDesc inputBuf;
    inputBuf.numBufs = 1;
    inputBuf.bufs = (void**)&inData;
    inputBuf.bufSizes = &inDataSize;
    inputBuf.bufElSizes = &inSampleSize;
    inputBuf.bufferIdentifiers = &inDataDescription;

    AACENC_InArgs inputDesc;
    inputDesc.numInSamples = sampleCount;
    inputDesc.numAncBytes = 0;
    // ======

    // === Output (result) Buffer ===
    AACENC_BufDesc outputBuf;
    outputBuf.numBufs = 1;
    outputBuf.bufs = (void**)&outData;
    outputBuf.bufSizes = &outDataSize;
    outputBuf.bufElSizes = &outElemSize;
    outputBuf.bufferIdentifiers = &outDataDescription;

    // Fed by aacEncEncode
    AACENC_OutArgs outputDesc;
    // ======

    int result = aacEncEncode(m_aacEnc, &inputBuf, &outputBuf, &inputDesc, &outputDesc);
    if(result != AACENC_OK) {
        kLogger.warning() << "aacEncEncode failed! error code:" << result;
        free(inData);
        free(outData);
        return;
    }

    m_pCallback->write(nullptr, outData, 0, outputDesc.numOutBytes);
    free(inData);
    free(outData);
}

void EncoderFdkAac::updateMetaData(const QString& artist, const QString& title, const QString& album) {
    (void)artist, (void)title, (void)album;
}

void EncoderFdkAac::flush() {
}
