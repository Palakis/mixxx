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
    EngineBroadcast(UserSettingsPointer pConfig,
                    BroadcastSettingsPointer pBroadcastSettings,
                    QSharedPointer<EngineNetworkStream> pNetworkStream);
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
    QSharedPointer<EngineNetworkStream> m_pNetworkStream;
    ControlProxy* m_pBroadcastEnabled;
};

#endif // ENGINE_SIDECHAIN_ENGINEBROADCAST_H
