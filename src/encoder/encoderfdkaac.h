// encoderfdkaac.h
// Created on Aug 15 2017 by Palakis

#ifndef ENCODER_ENCODERFDKAAC_H
#define ENCODER_ENCODERFDKAAC_H

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
    // libfdk-aac common AOTs
    static const int AOT_AAC_LC = 2; // AAC-LC
    static const int AOT_SBR = 5; // HE-AAC
    static const int AOT_PS = 29; // HE-AACv2

    // libfdk-aac types and structs
    typedef signed int INT;
    typedef unsigned int UINT;
    typedef signed short SHORT;
    typedef unsigned short USHORT;
    typedef signed char SCHAR;
    typedef unsigned char UCHAR;

    typedef enum {
        AACENC_OK = 0x0000,

        AACENC_INVALID_HANDLE = 0x0020,
        AACENC_MEMORY_ERROR = 0x0021,
        AACENC_UNSUPPORTED_PARAMETER = 0x0022,
        AACENC_INVALID_CONFIG = 0x0023,

        AACENC_INIT_ERROR = 0x0040,
        AACENC_INIT_AAC_ERROR = 0x0041,
        AACENC_INIT_SBR_ERROR = 0x0042,
        AACENC_INIT_TP_ERROR = 0x0043,
        AACENC_INIT_META_ERROR = 0x0044,

        AACENC_ENCODE_ERROR = 0x0060,

        AACENC_ENCODE_EOF = 0x0080,
    } AACENC_ERROR;

    typedef enum {
        AACENC_AOT = 0x0100,
        AACENC_BITRATE = 0x0101,
        AACENC_BITRATEMODE = 0x0102,
        AACENC_SAMPLERATE = 0x0103,
        AACENC_SBR_MODE = 0x0104,
        AACENC_GRANULE_LENGTH = 0x0105,
        AACENC_CHANNELMODE = 0x0106,
        AACENC_CHANNELORDER = 0x0107,
        AACENC_SBR_RATIO = 0x0108,
        AACENC_AFTERBURNER = 0x0200,
        AACENC_BANDWIDTH = 0x0203,
        AACENC_PEAK_BITRATE = 0x0207,
        AACENC_TRANSMUX = 0x0300,
        AACENC_HEADER_PERIOD = 0x0301,
        AACENC_SIGNALING_MODE = 0x0302,
        AACENC_TPSUBFRAMES = 0x0303,
        AACENC_AUDIOMUXVER = 0x0304,
        AACENC_PROTECTION = 0x0306,
        AACENC_ANCILLARY_BITRATE = 0x0500,
        AACENC_METADATA_MODE = 0x0600,
        AACENC_CONTROL_STATE = 0xFF00,
        AACENC_NONE = 0xFFFF
    } AACENC_PARAM;

    typedef enum {
        IN_AUDIO_DATA = 0,
        IN_ANCILLRY_DATA = 1,
        IN_METADATA_SETUP = 2,
        OUT_BITSTREAM_DATA = 3,
        OUT_AU_SIZES = 4
    } AACENC_BufferIdentifier;

    typedef struct AACENCODER *HANDLE_AACENCODER;
    typedef struct {
        UINT maxOutBufBytes;
        UINT maxAncBytes;
        UINT inBufFillLevel;
        UINT inputChannels;
        UINT frameLength;
        UINT encoderDelay;
        UCHAR confBuf[64];
        UINT confSize;
    } AACENC_InfoStruct;
    typedef struct {
        INT numBufs;
        void** bufs;
        INT* bufferIdentifiers;
        INT* bufSizes;
        INT* bufElSizes;
    } AACENC_BufDesc;
    typedef struct {
        INT numInSamples;
        INT numAncBytes;
    } AACENC_InArgs;
    typedef struct {
        INT numOutBytes;
        INT numInSamples;
        INT numAncBytes;
    } AACENC_OutArgs;

    // libfdk-aac functions prototypes
    typedef AACENC_ERROR (*aacEncOpen_)(
            HANDLE_AACENCODER*,
            const UINT,
            const UINT);

    typedef AACENC_ERROR (*aacEncClose_)(HANDLE_AACENCODER*);

    typedef AACENC_ERROR (*aacEncEncode_)(
            const HANDLE_AACENCODER,
            const AACENC_BufDesc*,
            const AACENC_BufDesc*,
            const AACENC_InArgs*,
            AACENC_OutArgs*);

    typedef AACENC_ERROR (*aacEncInfo_)(
            const HANDLE_AACENCODER,
            AACENC_InfoStruct*);

    typedef AACENC_ERROR (*aacEncoder_SetParam_)(
            const HANDLE_AACENCODER,
            const AACENC_PARAM,
            const UINT);

    // libfdk-aac function pointers
    aacEncOpen_ aacEncOpen;
    aacEncClose_ aacEncClose;
    aacEncEncode_ aacEncEncode;
    aacEncInfo_ aacEncInfo;
    aacEncoder_SetParam_ aacEncoder_SetParam;

    // Instance methods
    void processFIFO();

    // Instance attributes
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
