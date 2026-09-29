#include "BatteryState.h"

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Logging/Log.h"

#include <dbus/dbus.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace WallpaperEngine::System;

namespace {
constexpr std::chrono::seconds CacheTTL (5);
constexpr int UPowerTimeoutMs = 500;
} // namespace

bool BatteryState::isOnBattery () {
    const auto now = std::chrono::steady_clock::now ();

    if (this->m_haveCache && now - this->m_lastCheck < CacheTTL) {
	return this->m_cached;
    }

    bool upowerAnswer = false;

    if (this->queryUPower (upowerAnswer)) {
	this->m_cached = upowerAnswer;
    } else {
	this->m_cached = this->querySysfs ();
    }

    this->m_lastCheck = now;
    this->m_haveCache = true;

    return this->m_cached;
}

bool BatteryState::queryUPower (bool& out) {
    DBusError err;
    dbus_error_init (&err);

    DBusConnection* connection = dbus_bus_get (DBUS_BUS_SYSTEM, &err);

    if (connection == nullptr) {
	sLog.debugerror ("BatteryState: no system bus");
	dbus_error_free (&err);
	return false;
    }

    Data::Utils::ScopeGuard connectionGuard ([connection] { dbus_connection_unref (connection); });

    DBusMessage* msg = dbus_message_new_method_call (
	"org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "Get"
    );

    if (msg == nullptr) {
	return false;
    }

    Data::Utils::ScopeGuard msgGuard ([msg] { dbus_message_unref (msg); });

    const char* iface = "org.freedesktop.UPower";
    const char* prop = "OnBattery";

    dbus_message_append_args (msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block (connection, msg, UPowerTimeoutMs, &err);

    if (reply == nullptr) {
	sLog.debugerror ("BatteryState: UPower query failed, using sysfs fallback");
	dbus_error_free (&err);
	return false;
    }

    Data::Utils::ScopeGuard replyGuard ([reply] { dbus_message_unref (reply); });

    DBusMessageIter outer;
    dbus_message_iter_init (reply, &outer);

    if (dbus_message_iter_get_arg_type (&outer) != DBUS_TYPE_VARIANT) {
	return false;
    }

    DBusMessageIter variant;
    dbus_message_iter_recurse (&outer, &variant);

    if (dbus_message_iter_get_arg_type (&variant) != DBUS_TYPE_BOOLEAN) {
	return false;
    }

    dbus_bool_t value = FALSE;
    dbus_message_iter_get_basic (&variant, &value);
    out = value == TRUE;

    return true;
}

bool BatteryState::querySysfs () {
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::directory_iterator it ("/sys/class/power_supply", ec);

    if (ec) {
	return false;
    }

    for (const auto& entry : it) {
	std::ifstream typeFile (entry.path () / "type");

	if (!typeFile) {
	    continue;
	}

	std::string type;
	std::getline (typeFile, type);

	if (type != "Battery") {
	    continue;
	}

	std::ifstream statusFile (entry.path () / "status");

	if (!statusFile) {
	    continue;
	}

	std::string status;
	std::getline (statusFile, status);

	// sysfs values have no trailing newline after getline; trim CR just in case
	if (!status.empty () && status.back () == '\r') {
	    status.pop_back ();
	}

	if (status == "Discharging") {
	    return true;
	}
    }

    return false;
}
