#ifndef ENGINENETWORKSTREAM_H_
#define ENGINENETWORKSTREAM_H_

#include <QVector>

#include "util/types.h"
#include "util/fifo.h"
#include "engine/sidechain/networkstreamworker.h"

class EngineNetworkStream {
  public:
    EngineNetworkStream(int numOutputChannels,
            int numInputChannels);
    virtual ~EngineNetworkStream();

    void startStream(double sampleRate);
    void stopStream();

    int getWriteExpected();
    int getReadExpected();

    void write(const CSAMPLE* buffer, int frames);
    void read(CSAMPLE* buffer, int frames);
    void writeSilence(int frames);

    void writingDone(int interval);

    qint64 getStreamTimeUs();
    qint64 getStreamTimeFrames();

    int getNumOutputChannels() {
        return m_numOutputChannels;
    }

    int getNumInputChannels() {
        return m_numInputChannels;
    }

    static qint64 getNetworkTimeUs();

    void addWorker(QSharedPointer<NetworkStreamWorker> pWorker);
    void removeWorker(QSharedPointer<NetworkStreamWorker> pWorker);

  private:
    int nextListSlotAvailable();
    void debugFreeSlots();

    FIFO<CSAMPLE>* m_pOutputFifo;
    FIFO<CSAMPLE>* m_pInputFifo;
    int m_numOutputChannels;
    int m_numInputChannels;
    double m_sampleRate;
    qint64 m_streamStartTimeUs;
    qint64 m_streamFramesWritten;
    qint64 m_streamFramesRead;
    int m_writeOverflowCount;

    // EngineNetworkStream can't use locking mechanisms to protect its
    // internal worker list against concurrency issues, as it is used by
    // methods called from the audio engine thread.
    // Instead, the internal list has a fixed number of QSharedPointers
    // (which are thread-safe) initialized with null pointers. R/W operations to
    // the workers are then performed on thread-safe QSharedPointers and not
    // onto the thread-unsafe QVector
    QVector<QSharedPointer<NetworkStreamWorker>> m_workers;
};

#endif /* ENGINENETWORKSTREAM_H_ */
