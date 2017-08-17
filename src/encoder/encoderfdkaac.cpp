// encoderfdkaac.cpp
// Created on Aug 15 2017 by Palakis

#include <QDesktopServices>
#include <QDir>
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
    : aacEncOpen(nullptr),
      aacEncClose(nullptr),
      aacEncEncode(nullptr),
      aacEncInfo(nullptr),
      aacEncoder_SetParam(nullptr),
      m_aacAot(0),
      m_bitrate(0),
      m_channels(0),
      m_samplerate(0),
      m_pCallback(pCallback),
      m_library(nullptr),
      m_pInputBuffer(nullptr),
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
    libnames << "libfdk-aac-1.dll";

    QString buttFdkAacPath = buttWindowsFdkAac();
    if(!buttFdkAacPath.isNull()) {
        libnames << buttFdkAacPath;
    }
#elif __APPLE__
    // Using Homebrew ('brew install fdk-aac' command):
    libnames << "/usr/local/lib/libfdk-aac.dylib";
    // Using MacPorts ('sudo port install libfdk-aac' command):
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

    aacEncGetLibInfo = (aacEncGetLibInfo_)m_library->resolve("aacEncGetLibInfo");
    aacEncOpen = (aacEncOpen_)m_library->resolve("aacEncOpen");
    aacEncClose = (aacEncClose_)m_library->resolve("aacEncClose");
    aacEncEncode = (aacEncEncode_)m_library->resolve("aacEncEncode");
    aacEncInfo = (aacEncInfo_)m_library->resolve("aacEncInfo");
    aacEncoder_SetParam = (aacEncoder_SetParam_)m_library->resolve("aacEncoder_SetParam");

    // Check if all function pointers aren't null.
    // Otherwise, the version of libfdk-aac loaded doesn't comply with the official distribution
    // Shouldn't happen on Linux, mainly on Windows.
    if(!aacEncGetLibInfo ||
       !aacEncOpen ||
       !aacEncClose ||
       !aacEncEncode ||
       !aacEncInfo ||
       !aacEncoder_SetParam)
    {
        m_library->unload();
        delete m_library;
        m_library = nullptr;

        kLogger.debug() << "aacEncGetLibInfo:" << aacEncGetLibInfo;
        kLogger.debug() << "aacEncOpen:" << aacEncOpen;
        kLogger.debug() << "aacEncClose:" << aacEncClose;
        kLogger.debug() << "aacEncEncode:" << aacEncEncode;
        kLogger.debug() << "aacEncInfo:" << aacEncInfo;
        kLogger.debug() << "aacEncoder_SetParam:" << aacEncoder_SetParam;

        ErrorDialogProperties* props = ErrorDialogHandler::instance()->newDialogProperties();
        props->setType(DLG_WARNING);
        props->setTitle(QObject::tr("Encoder"));
        QString key = QObject::tr(
                "<html>Mixxx has detected that you use a modified version of libfdk-aac. "
                "See <a href='http://mixxx.org/wiki/doku.php/internet_broadcasting'>Mixxx Wiki</a> "
                "for more information.</html>");
        props->setText(key);
        props->setKey(key);
        ErrorDialogHandler::instance()->requestErrorDialog(props);
        return;
    }

    kLogger.debug() << "Loaded libfdk-aac";
}

EncoderFdkAac::~EncoderFdkAac() {
    aacEncClose(&m_aacEnc);
    if (m_library && m_library->isLoaded()) {
        flush();
        m_library->unload();
        delete m_library;
        kLogger.debug() << "Unloaded libfdk-aac";
    }

    if (m_pInputBuffer) {
        delete m_pInputBuffer;
    }
}

// TODO(Palakis): test this on Windows
QString EncoderFdkAac::buttWindowsFdkAac() {
    QString appData = QDesktopServices::storageLocation(
            QDesktopServices::DataLocation);
    appData = QFileInfo(appData).absolutePath();

    // Candidate paths for a butt installation
    QStringList searchPaths;
    searchPaths << "C:\\Program Files";
    searchPaths << "C:\\Program Files (x86)";
    searchPaths << appData;
    searchPaths << (appData + "\\Local");

    // Try to find a butt installation in one of the
    // potential paths above
    for (QString candidate : searchPaths) {
        QDir folder(candidate);
        if(!folder.exists()) {
            continue;
        }

        // List subfolders
        QStringList nameFilters("butt*");
        QStringList subfolders =
                folder.entryList(nameFilters, QDir::Dirs, QDir::Name);

        // If a butt installation is found, try
        // to find libfdk-aac in it
        for (QString subName : subfolders) {
            if(!folder.cd(subName)) {
                continue;
            }

            QString libFile = "libfdk-aac-1.dll";
            if(!folder.exists(libFile)) {
                // Found a libfdk-aac here.
                // Return the full path of the .dll file.
                return folder.absoluteFilePath(libFile);
            }

            folder.cdUp();
        }
    }

    return QString::null;
}

void EncoderFdkAac::setEncoderSettings(const EncoderSettings& settings) {
    // TODO(Palakis): support more bitrate configurations
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

int EncoderFdkAac::initEncoder(int samplerate, QString errorMessage) {
    (void)errorMessage;
    m_samplerate = samplerate;

    if(!m_library) {
        kLogger.warning() << "initEncoder failed: fdk-aac library not loaded";
        return -1;
    }

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

    // Output bitrate in bits per second
    // m_bitrate is in kilobits per second, conversion needed
    aacEncoder_SetParam(m_aacEnc, AACENC_BITRATE, m_bitrate * 1000);
    // Transport type (2 = ADTS)
    aacEncoder_SetParam(m_aacEnc, AACENC_TRANSMUX, 2);
    // Enable the AAC Afterburner, which increases audio quality
    // at the cost of increased CPU and memory usage.
    // Fraunhofer recommends to enable this if increased CPU and memory
    // consumption is not a problem.
    // TODO(Palakis): is this an issue even with 12-year old computers
    // and notebooks?
    aacEncoder_SetParam(m_aacEnc, AACENC_AFTERBURNER, 1);

    // Actual encoder init, validates settings provided above
    int result = aacEncEncode(m_aacEnc, nullptr, nullptr, nullptr, nullptr);
    if (result != AACENC_OK) {
        kLogger.warning() << "aac encoder init failed! error code:" << result;
        return -1;
    }

    aacEncInfo(m_aacEnc, &m_aacInfo);
    // TODO(Palakis): use constant or get value from engine
    m_pInputBuffer = new FIFO<CSAMPLE>(57344 * 2);
    m_pChunkBuffer = (CSAMPLE*)malloc(
        m_aacInfo.frameLength * m_channels * sizeof(CSAMPLE));
    return 0;
}

void EncoderFdkAac::encodeBuffer(const CSAMPLE *samples, const int sampleCount) {
    if (!m_pInputBuffer) {
        return;
    }

    int writeRequired = sampleCount;
    int writeAvailable = m_pInputBuffer->writeAvailable();
    if (writeRequired > writeAvailable) {
        kLogger.warning() << "FIFO buffer too small, loosing samples!"
                          << "required:" << writeRequired
                          << "; available: " << writeAvailable;
    }

    int writeCount = math_min(writeRequired, writeAvailable);
    if (writeCount > 0) {
        m_pInputBuffer->write(samples, sampleCount);
    }

    processFIFO();
}

void EncoderFdkAac::processFIFO() {
    if (!m_pInputBuffer || !m_pChunkBuffer) {
        return;
    }

    int readRequired = m_aacInfo.frameLength * m_channels;
    while (m_pInputBuffer->readAvailable() >= readRequired) {
        memset(m_pChunkBuffer, 0, readRequired * sizeof(CSAMPLE));
        m_pInputBuffer->read(m_pChunkBuffer, readRequired);

        // fdk-aac only accept pointers for most buffer settings.
        // Declare settings here and point to them below.
        int inSampleSize = sizeof(SAMPLE);
        int inDataSize = readRequired * inSampleSize;
        SAMPLE* inData = (SAMPLE*)malloc(inDataSize);
        // fdk-aac doesn't support float samples, so convert
        // to integers instead
        SampleUtil::convertFloat32ToS16(inData, m_pChunkBuffer, readRequired);
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
        inputDesc.numInSamples = readRequired;
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
        if (result != AACENC_OK) {
            kLogger.warning() << "aacEncEncode failed! error code:" << result;
            free(inData);
            free(outData);
            return;
        }

        int sampleDiff = inputDesc.numInSamples - outputDesc.numInSamples;
        if (sampleDiff > 0) {
            kLogger.warning() << "encoder ignored" << sampleDiff << "samples!";
        }

        m_pCallback->write(nullptr, outData, 0, outputDesc.numOutBytes);
        free(inData);
        free(outData);
    }
}

void EncoderFdkAac::updateMetaData(const QString& artist, const QString& title, const QString& album) {
    (void)artist, (void)title, (void)album;
}

void EncoderFdkAac::flush() {
    // At this point there may still be samples in the FIFO buffer.
    processFIFO();
}
