#include "engine/sidechain/enginenetworkstream.h"
#include "util/sample.h"

#include "networkstreamworker.h"

namespace {
const int kNetworkLatencyFrames = 8192; // 185 ms @ 44100 Hz
// Related chunk sizes:
// Mp3 frames = 1152 samples
// Ogg frames = 64 to 8192 samples.
// In Mixxx 1.11 we transmit every decoder-frames at once,
// Which results in case of ogg in a dynamic latency from 0.14 ms to to 185 ms
// Now we have switched to a fixed latency of 8192 frames (stereo samples) =
// which is 185 @ 44100 ms and twice the maximum of the max mixxx audio buffer
const int kBufferFrames = kNetworkLatencyFrames * 4; // 743 ms @ 44100 Hz
// normally * 2 is sufficient.
// We allow to buffer two extra chunks for a CPU overload case, when
// the broadcast thread is not scheduled in time.
}

NetworkStreamWorker::NetworkStreamWorker()
    : m_networkStreamWorkerState(NETWORKSTREAMWORKER_STATE_NEW),
      m_functionCode(0),
      m_runCount(0),
      m_streamStartTimeUs(-1),
      m_streamFramesWritten(0),
      m_writeOverflowCount(0),
      m_sampleRate(0),
      m_numOutputChannels(0),
      m_outputDrift(false) {
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

void NetworkStreamWorker::startStream(double samplerate, int numOutputChannels) {
    m_sampleRate = samplerate;
    m_numOutputChannels = numOutputChannels;

    m_streamStartTimeUs = EngineNetworkStream::getNetworkTimeUs();
    m_streamFramesWritten = 0;
}

void NetworkStreamWorker::stopStream() {
    m_sampleRate = 0;
    m_numOutputChannels = 0;

    m_streamStartTimeUs = -1;
}

void NetworkStreamWorker::processWrite(int outChunkSize, int readAvailable,
        CSAMPLE* dataPtr1, ring_buffer_size_t size1,
        CSAMPLE* dataPtr2, ring_buffer_size_t size2) {
    int writeAvailable = getWriteExpected() * m_numOutputChannels;
    int copyCount = qMin(readAvailable, writeAvailable);

    qDebug() << "SoundDeviceNetwork::writeProcess: writeAvailable: " << writeAvailable;
    qDebug() << "SoundDeviceNetwork::writeProcess: readAvailable: " << readAvailable;
    qDebug() << "SoundDeviceNetwork::writeProcess: copyCount: " << copyCount;

    //qDebug() << "SoundDevicePortAudio::writeProcess()" << toRead << writeAvailable;
    if (copyCount > 0) {

        if (writeAvailable >= outChunkSize * 2) {
            // Underflow
            //qDebug() << "SoundDeviceNetwork::writeProcess() Buffer empty";
            // catch up by filling buffer until we are synced
            writeSilence(writeAvailable - copyCount);
        } else if (writeAvailable > readAvailable + outChunkSize / 2) {
            // try to keep PAs buffer filled up to 0.5 chunks
            if (m_outputDrift) {
                // duplicate one frame
                //qDebug() << "SoundDeviceNetwork::writeProcess() duplicate one frame"
                //         << (float)writeAvailable / outChunkSize << (float)readAvailable / outChunkSize;
                write(dataPtr1, 1);
            } else {
                m_outputDrift = true;
            }
        } else if (writeAvailable < outChunkSize / 2) {
            // We are not able to store all new frames
            if (m_outputDrift) {
                //qDebug() << "SoundDeviceNetwork::writeProcess() skip one frame"
                //         << (float)writeAvailable / outChunkSize << (float)readAvailable / outChunkSize;
                ++copyCount;
            } else {
                m_outputDrift = true;
            }
        } else {
            m_outputDrift = false;
        }

        write(dataPtr1, size1 / m_numOutputChannels);
        if (size2 > 0) {
            write(dataPtr2, size2 / m_numOutputChannels);
        }

        writingDone(copyCount);
    }
}

int NetworkStreamWorker::getWriteExpected() {
    return static_cast<int>(getStreamTimeFrames() - m_streamFramesWritten);
}

void NetworkStreamWorker::write(const CSAMPLE* buffer, int frames) {
    if (!threadWaiting()) {
        m_streamFramesWritten += frames;
        return;
    }

    QSharedPointer<FIFO<CSAMPLE>> pFifo = getOutputFifo();
    if (pFifo) {
        int writeAvailable = pFifo->writeAvailable();
        int writeRequired = frames * m_numOutputChannels;
        if (writeAvailable < writeRequired) {
            qDebug() << "NetworkStreamWorker::write: worker buffer full, loosing samples";
            m_writeOverflowCount++;
        }

        int copyCount = math_min(writeAvailable, writeRequired);
        if (copyCount > 0) {
            (void)pFifo->write(buffer, copyCount);
            // we advance the frame only by the samples we have actually copied
            // This means in case of buffer full (where we loose some frames)
            // we do not get out of sync, and the syncing code tries to catch up the
            // stream by writing silence, once the buffer is free.
            m_streamFramesWritten += copyCount / m_numOutputChannels;
        }
    }
}

void NetworkStreamWorker::writeSilence(int frames) {
    if (!threadWaiting()) {
        m_streamFramesWritten += frames;
        return;
    }

    QSharedPointer<FIFO<CSAMPLE>> pFifo = getOutputFifo();
    if(pFifo) {
        int writeAvailable = pFifo->writeAvailable();
        int writeRequired = frames * m_numOutputChannels;
        if (writeAvailable < writeRequired) {
            qDebug() <<
                    "NetworkStreamWorker::writeSilence: worker buffer full, loosing samples";
            m_writeOverflowCount++;
        }

        int clearCount = math_min(writeAvailable, writeRequired);
        if (clearCount > 0) {
            CSAMPLE* dataPtr1;
            ring_buffer_size_t size1;
            CSAMPLE* dataPtr2;
            ring_buffer_size_t size2;

            (void)pFifo->aquireWriteRegions(clearCount,
                    &dataPtr1, &size1, &dataPtr2, &size2);
            SampleUtil::clear(dataPtr1, size1);
            if (size2 > 0) {
                SampleUtil::clear(dataPtr2, size2);
            }
            pFifo->releaseWriteRegions(clearCount);

            // we advance the frame only by the samples we have actually cleared
            m_streamFramesWritten += clearCount / m_numOutputChannels;
        }
    }
}

void NetworkStreamWorker::writingDone(int interval) {
    QSharedPointer<FIFO<CSAMPLE>> pFifo = getOutputFifo();
    if (!pFifo) {
        return;
    }

    // Check for desired kNetworkLatencyFrames + 1/2 interval to
    // avoid big jitter due to interferences with sync code
    if (pFifo->readAvailable() + interval / 2
            >= (m_numOutputChannels * kNetworkLatencyFrames)) {
        outputAvailable();
    }
}

bool NetworkStreamWorker::threadWaiting() {
    return false;
}

qint64 NetworkStreamWorker::getStreamTimeFrames() {
    return static_cast<double>(getStreamTimeUs()) * m_sampleRate / 1000000.0;
}

qint64 NetworkStreamWorker::getStreamTimeUs() {
    return EngineNetworkStream::getNetworkTimeUs() - m_streamStartTimeUs;
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
