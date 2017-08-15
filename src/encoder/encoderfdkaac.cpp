// encoderfdkaac.cpp
// Created on Aug 15 2017 by Palakis

#include <QString>
#include <QStringList>

#include "util/logger.h"

#include "encoder/encoderfdkaac.h"

namespace {
const mixxx::Logger kLogger("EncoderFdkAac");
}

EncoderFdkAac::EncoderFdkAac(EncoderCallback* pCallback, const char* pFormat)
    : m_pFormat(pFormat),
      m_bitrate(0),
      m_channels(0),
      m_samplerate(0),
      m_pCallback(pCallback) {

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
    m_samplerate = samplerate;
    return -1;
}

void EncoderFdkAac::encodeBuffer(const CSAMPLE *samples, const int size) {

}

void EncoderFdkAac::updateMetaData(const QString& artist, const QString& title, const QString& album) {

}

void EncoderFdkAac::flush() {

}
