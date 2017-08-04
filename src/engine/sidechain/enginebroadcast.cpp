#include <QtDebug>

#include <signal.h>

// shout.h checks for WIN32 to see if we are on Windows.
#ifdef WIN64
#define WIN32
#endif
#include <shout/shout.h>
#ifdef WIN64
#undef WIN32
#endif

#include "broadcast/defs_broadcast.h"
#include "control/controlpushbutton.h"
#include "preferences/usersettings.h"
#include "util/sample.h"

#include "engine/sidechain/enginebroadcast.h"

EngineBroadcast::EngineBroadcast(UserSettingsPointer pConfig,
                                 BroadcastSettingsPointer pBroadcastSettings,
                                 const std::unique_ptr<EngineNetworkStream>& pNetworkStream)
        : m_settings(pBroadcastSettings),
          m_pConfig(pConfig),
          m_pNetworkStream(pNetworkStream) {
    const bool persist = true;

    m_pBroadcastEnabled = new ControlPushButton(
            ConfigKey(BROADCAST_PREF_KEY,"enabled"), persist);
    m_pBroadcastEnabled->setButtonMode(ControlPushButton::TOGGLE);
    connect(m_pBroadcastEnabled, SIGNAL(valueChanged(double)),
            this, SLOT(slotEnableCO(double)));

    m_pStatusCO = new ControlObject(ConfigKey(BROADCAST_PREF_KEY, "status"));
    m_pStatusCO->setReadOnly();
    m_pStatusCO->forceSet(STATUSCO_UNCONNECTED);

    // Initialize connections list from the current state of BroadcastSettings
    QList<BroadcastProfilePtr> profiles = m_settings->profiles();
    for(BroadcastProfilePtr profile : profiles) {
        addConnection(profile);
    }

    // Connect add/remove/renamed profiles signals.
    // Passing the raw pointer from QSharedPointer to connect() is fine, since
    // connect is trusted that it won't delete the pointer
    connect(m_settings.data(), SIGNAL(profileAdded(BroadcastProfilePtr)),
            this, SLOT(slotProfileAdded(BroadcastProfilePtr)));
    connect(m_settings.data(), SIGNAL(profileRemoved(BroadcastProfilePtr)),
            this, SLOT(slotProfileRemoved(BroadcastProfilePtr)));
    connect(m_settings.data(), SIGNAL(profileRenamed(QString, BroadcastProfilePtr)),
            this, SLOT(slotProfileRenamed(QString, BroadcastProfilePtr)));
    connect(m_settings.data(), SIGNAL(profilesChanged()),
            this, SLOT(slotProfilesChanged()));

    // Initialize libshout
    shout_init();
}

EngineBroadcast::~EngineBroadcast() {
    delete m_pStatusCO;

    m_pBroadcastEnabled->set(0);

    shout_shutdown();
}

bool EngineBroadcast::addConnection(BroadcastProfilePtr profile) {
    if(!profile)
        return false;

    QString profileName = profile->getProfileName();

    if(m_connections.contains(profileName))
        return false;

    ShoutConnectionPtr output(new ShoutConnection(profile, m_pConfig));

    m_connectionsMutex.lock();
    m_connections.insert(profileName, output);
    m_pNetworkStream->addWorker(output);
    m_connectionsMutex.unlock();

    qDebug() << "EngineBroadcast::addConnection: created connection for profile" << profileName;
    return true;
}

bool EngineBroadcast::removeConnection(BroadcastProfilePtr profile) {
    if(!profile)
        return false;

    m_connectionsMutex.lock();
    ShoutConnectionPtr output = m_connections.take(profile->getProfileName());
    m_connectionsMutex.unlock();

    if(output) {
        // Disabling the profile tells ShoutOutput's thread to disconnect
        output->profile()->setEnabled(false);
        m_pNetworkStream->removeWorker(output);

        qDebug() << "EngineBroadcast::removeConnection: removed connection for profile" << profile->getProfileName();
        return true;
    }

    return false;
}

void EngineBroadcast::slotEnableCO(double v) {
    if (v > 1.0) {
        // Wrap around manually .
        // Wrapping around in WPushbutton does not work
        // since the status button has 4 states, but this CO is bool
        m_pBroadcastEnabled->set(0.0);
        v = 0.0;
    }

    if (v > 0.0) {
        slotProfilesChanged();
        m_pStatusCO->forceSet(STATUSCO_CONNECTED);
    } else {
        m_pStatusCO->forceSet(STATUSCO_UNCONNECTED);
    }
}

void EngineBroadcast::slotProfileAdded(BroadcastProfilePtr profile) {
    addConnection(profile);
}

void EngineBroadcast::slotProfileRemoved(BroadcastProfilePtr profile) {
    removeConnection(profile);
}

void EngineBroadcast::slotProfileRenamed(QString oldName, BroadcastProfilePtr profile) {
    ShoutConnectionPtr oldItem = m_connections.take(oldName);
    if(oldItem) {
        // Profile in ShoutOutput is a reference, which is supposed
        // to have already been updated
        QString newName = profile->getProfileName();
        m_connections.insert(newName, oldItem);
    }
}

void EngineBroadcast::slotProfilesChanged() {
    if(m_pBroadcastEnabled->toBool()) {
        for(ShoutConnectionPtr c : m_connections.values()) {
            if(c) c->applySettings();
        }
    }
}
