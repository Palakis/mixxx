#ifndef ENGINE_SIDECHAIN_ENGINEBROADCAST_H
#define ENGINE_SIDECHAIN_ENGINEBROADCAST_H

#include <QMessageBox>
#include <QMutex>
#include <QObject>
#include <QMap>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "preferences/usersettings.h"
#include "preferences/broadcastsettings.h"
#include "engine/sidechain/enginenetworkstream.h"
#include "engine/sidechain/shoutconnection.h"

class ControlPushButton;

class EngineBroadcast : public QObject {
    Q_OBJECT
  public:
    enum StatusCOStates {
        STATUSCO_UNCONNECTED = 0, // IDLE state, no error
        STATUSCO_CONNECTING = 1, // 30 s max
        STATUSCO_CONNECTED = 2, // On Air
        STATUSCO_FAILURE = 3 // Happens when disconnected by an error
    };

    EngineBroadcast(UserSettingsPointer pConfig,
                    BroadcastSettingsPointer pBroadcastSettings,
                    const std::unique_ptr<EngineNetworkStream>& pNetworkStream);
    virtual ~EngineBroadcast();

    bool addConnection(BroadcastProfilePtr profile);
    bool removeConnection(BroadcastProfilePtr profile);

  private slots:
    void slotEnableCO(double v);
    void slotProfileAdded(BroadcastProfilePtr profile);
    void slotProfileRemoved(BroadcastProfilePtr profile);
    void slotProfileRenamed(QString oldName, BroadcastProfilePtr profile);
    void slotProfilesChanged();

  private:
    QMap<QString,ShoutConnectionPtr> m_connections;
    QMutex m_connectionsMutex;

    BroadcastSettingsPointer m_settings;
    UserSettingsPointer m_pConfig;
    const std::unique_ptr<EngineNetworkStream>& m_pNetworkStream;

    ControlPushButton* m_pBroadcastEnabled;
    ControlObject* m_pStatusCO;
};

#endif // ENGINE_SIDECHAIN_ENGINEBROADCAST_H
