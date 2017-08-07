#ifndef BROADCAST_BROADCASTMANAGER_H
#define BROADCAST_BROADCASTMANAGER_H

#include <QObject>

#include "engine/sidechain/enginebroadcast.h"
#include "preferences/settingsmanager.h"
#include "preferences/usersettings.h"

class SoundManager;

class BroadcastManager : public QObject {
    Q_OBJECT
  public:
    enum StatusCOStates {
        STATUSCO_UNCONNECTED = 0, // IDLE state, no error
        STATUSCO_CONNECTING = 1, // 30 s max
        STATUSCO_CONNECTED = 2, // On Air
        STATUSCO_FAILURE = 3 // Happens when disconnected by an error
    };

    BroadcastManager(SettingsManager* pSettingsManager,
                     SoundManager* pSoundManager);
    virtual ~BroadcastManager();

    // Returns true if the broadcast connection is enabled. Note this only
    // indicates whether the connection is enabled, not whether it is connected.
    bool isEnabled();

  public slots:
    // Set whether or not the Broadcast connection is enabled.
    void setEnabled(bool enabled);

  signals:
    void broadcastEnabled(bool);

  private slots:
    void slotControlEnabled(double v);

  private:
    UserSettingsPointer m_pConfig;
    QSharedPointer<EngineBroadcast> m_pBroadcast;
    ControlPushButton* m_pBroadcastEnabled;
    ControlObject* m_pStatusCO;
};

#endif /* BROADCAST_BROADCASTMANAGER_H */
