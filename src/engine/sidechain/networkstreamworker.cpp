#include "networkstreamworker.h"

NetworkStreamWorker::NetworkStreamWorker()
    : m_networkStreamWorkerState(NETWORKSTREAMWORKER_STATE_NEW),
      m_functionCode(0),
      m_runCount(0),
      m_streamStartTimeUs(-1),
      m_streamFramesWritten(0) {
}

NetworkStreamWorker::~NetworkStreamWorker() {
}

void NetworkStreamWorker::outputAvailable() {
}

void NetworkStreamWorker::setOutputFifo(QSharedPointer<FIFO<CSAMPLE>> pOutputFifo) {
    Q_UNUSED(pOutputFifo);
}

QSharedPointer<FIFO<CSAMPLE>> NetworkStreamWorker::getOutputFifo() {
    return QSharedPointer<FIFO<CSAMPLE>>();
}

bool NetworkStreamWorker::threadWaiting() {
    return false;
}

void NetworkStreamWorker::setStartTime(qint64 startTime) {
    m_streamStartTimeUs = startTime;
}

qint64 NetworkStreamWorker::getStartTime() {
    return m_streamStartTimeUs;
}

void NetworkStreamWorker::resetFramesWritten() {
    m_streamFramesWritten = 0;
}

void NetworkStreamWorker::addFramesWritten(qint64 frames) {
    m_streamFramesWritten += frames;
}

qint64 NetworkStreamWorker::getFramesWritten() {
    return m_streamFramesWritten;
}

int NetworkStreamWorker::getState() {
    return m_networkStreamWorkerState;
}

int NetworkStreamWorker::getFunctionCode() {
    return m_functionCode;
}

int NetworkStreamWorker::getRunCount() {
    return m_runCount;
}

void NetworkStreamWorker::debugState() {
    qDebug() << "NetworkStreamWorker state:"
             << m_networkStreamWorkerState
             << m_functionCode
             << m_runCount;
}

void NetworkStreamWorker::setState(int state) {
    m_networkStreamWorkerState = state;
}

void NetworkStreamWorker::setFunctionCode(int code) {
    m_functionCode = code;
}

void NetworkStreamWorker::incRunCount() {
    m_runCount++;
}
