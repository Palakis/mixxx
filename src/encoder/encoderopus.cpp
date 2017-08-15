// encoderopus.cpp
// Create on August 15th 2017 by Palakis

#include "encoder/encoderopus.h"

EncoderOpus::EncoderOpus(EncoderCallback* pCallback)
    : m_pCallback(pCallback) {
}

EncoderOpus::~EncoderOpus() {
}

int EncoderOpus::initEncoder(int samplerate, QString errorMessage) {
    return 0;
}

void EncoderOpus::encodeBuffer(const CSAMPLE *samples, const int size) {

}

void EncoderOpus::updateMetaData(const QString& artist, const QString& title, const QString& album) {

}

void EncoderOpus::flush() {

}

void EncoderOpus::setEncoderSettings(const EncoderSettings& settings) {

}
