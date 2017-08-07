// shout.h checks for WIN32 to see if we are on Windows.
#ifdef WIN64
#define WIN32
#endif
#include <shout/shout.h>
#ifdef WIN64
#undef WIN32
#endif

#include "broadcast/defs_broadcast.h"
#include "engine/enginemaster.h"
#include "engine/sidechain/enginenetworkstream.h"
#include "engine/sidechain/enginesidechain.h"
#include "soundio/soundmanager.h"

#include "broadcast/broadcastmanager.h"

BroadcastManager::BroadcastManager(SettingsManager* pSettingsManager,
                                   SoundManager* pSoundManager)
        : m_pConfig(pSettingsManager->settings()) {
    const bool persist = true;
    m_pBroadcastEnabled = new ControlPushButton(
            ConfigKey(BROADCAST_PREF_KEY,"enabled"), persist);
    m_pBroadcastEnabled->setButtonMode(ControlPushButton::TOGGLE);
    connect(m_pBroadcastEnabled, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlEnabled(double)));

    m_pStatusCO = new ControlObject(ConfigKey(BROADCAST_PREF_KEY, "status"));
    m_pStatusCO->setReadOnly();
    m_pStatusCO->forceSet(STATUSCO_UNCONNECTED);

    // Initialize libshout
    shout_init();

    QSharedPointer<EngineNetworkStream> pNetworkStream =
            pSoundManager->getNetworkStream();
    if (pNetworkStream) {
        m_pBroadcast = QSharedPointer<EngineBroadcast>(
                new EngineBroadcast(m_pConfig,
                                    pSettingsManager->broadcastSettings(),
                                    pNetworkStream));
    }
}

BroadcastManager::~BroadcastManager() {
    // Disable broadcast so when Mixxx starts again it will not connect.
    m_pBroadcastEnabled->set(0);

    delete m_pStatusCO;
    delete m_pBroadcastEnabled;

    shout_shutdown();
}

void BroadcastManager::setEnabled(bool value) {
    m_pBroadcastEnabled->set(value);
}

bool BroadcastManager::isEnabled() {
    return m_pBroadcastEnabled->toBool();
}

void BroadcastManager::slotControlEnabled(double v) {
    if (v > 1.0) {
        // Wrap around manually .
        // Wrapping around in WPushbutton does not work
        // since the status button has 4 states, but this CO is bool
        m_pBroadcastEnabled->set(0.0);
        v = 0.0;
    }

    if (v > 0.0) {
        m_pStatusCO->forceSet(STATUSCO_CONNECTED);
    } else {
        m_pStatusCO->forceSet(STATUSCO_UNCONNECTED);
    }

    emit(broadcastEnabled(v > 0.0));
}
