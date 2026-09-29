#include "MPRISServer.h"

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Media;

namespace {
constexpr const char* BusName = "org.mpris.MediaPlayer2.linux-wallpaperengine";
constexpr const char* ObjectPath = "/org/mpris/MediaPlayer2";
constexpr const char* IfaceRoot = "org.mpris.MediaPlayer2";
constexpr const char* IfacePlayer = "org.mpris.MediaPlayer2.Player";
constexpr const char* IfaceProps = "org.freedesktop.DBus.Properties";
constexpr const char* IfaceIntro = "org.freedesktop.DBus.Introspectable";

constexpr const char* IntrospectionXml
    = "<node>"
      "<interface name=\"org.mpris.MediaPlayer2\">"
      "<method name=\"Raise\"/><method name=\"Quit\"/>"
      "<property name=\"Identity\" type=\"s\" access=\"read\"/>"
      "<property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\"/>"
      "<property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/>"
      "<property name=\"CanQuit\" type=\"b\" access=\"read\"/>"
      "<property name=\"CanRaise\" type=\"b\" access=\"read\"/>"
      "<property name=\"HasTrackList\" type=\"b\" access=\"read\"/>"
      "</interface>"
      "<interface name=\"org.mpris.MediaPlayer2.Player\">"
      "<method name=\"Next\"/><method name=\"Previous\"/><method name=\"Pause\"/>"
      "<method name=\"PlayPause\"/><method name=\"Play\"/><method name=\"Stop\"/>"
      "<property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>"
      "<property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>"
      "<property name=\"CanGoNext\" type=\"b\" access=\"read\"/>"
      "<property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>"
      "<property name=\"CanPlay\" type=\"b\" access=\"read\"/>"
      "<property name=\"CanPause\" type=\"b\" access=\"read\"/>"
      "<property name=\"CanSeek\" type=\"b\" access=\"read\"/>"
      "<property name=\"CanControl\" type=\"b\" access=\"read\"/>"
      "</interface>"
      "<interface name=\"org.freedesktop.DBus.Properties\">"
      "<method name=\"Get\"><arg name=\"interface\" type=\"s\" direction=\"in\"/>"
      "<arg name=\"property\" type=\"s\" direction=\"in\"/>"
      "<arg name=\"value\" type=\"v\" direction=\"out\"/></method>"
      "<method name=\"GetAll\"><arg name=\"interface\" type=\"s\" direction=\"in\"/>"
      "<arg name=\"properties\" type=\"a{sv}\" direction=\"out\"/></method>"
      "</interface>"
      "<interface name=\"org.freedesktop.DBus.Introspectable\">"
      "<method name=\"Introspect\"><arg name=\"data\" type=\"s\" direction=\"out\"/></method>"
      "</interface>"
      "</node>";

bool safeCall (const std::function<void ()>& callback) {
    if (!callback) {
	return false;
    }

    try {
	callback ();
    } catch (const std::exception& e) {
	sLog.error ("MPRIS callback failed: ", e.what ());
	return false;
    }

    return true;
}

template <typename T> T safeQuery (const std::function<T ()>& callback, T fallback) {
    if (!callback) {
	return fallback;
    }

    try {
	return callback ();
    } catch (const std::exception& e) {
	sLog.error ("MPRIS query failed: ", e.what ());
	return fallback;
    }
}

// --- reply builders (all append to an \(\{sv\}\) dict or a single variant) ---

void appendString (DBusMessageIter* dict, const char* key, const char* value) {
    DBusMessageIter entry, variant;
    const char* signature = "s";

    dbus_message_iter_open_container (dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic (&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container (&entry, DBUS_TYPE_VARIANT, signature, &variant);
    dbus_message_iter_append_basic (&variant, DBUS_TYPE_STRING, &value);
    dbus_message_iter_close_container (&entry, &variant);
    dbus_message_iter_close_container (dict, &entry);
}

void appendBool (DBusMessageIter* dict, const char* key, dbus_bool_t value) {
    DBusMessageIter entry, variant;
    const char* signature = "b";

    dbus_message_iter_open_container (dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic (&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container (&entry, DBUS_TYPE_VARIANT, signature, &variant);
    dbus_message_iter_append_basic (&variant, DBUS_TYPE_BOOLEAN, &value);
    dbus_message_iter_close_container (&entry, &variant);
    dbus_message_iter_close_container (dict, &entry);
}

void appendStringArray (DBusMessageIter* dict, const char* key, const char* const* values, int count) {
    DBusMessageIter entry, variant, array;
    const char* signature = "as";

    dbus_message_iter_open_container (dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic (&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container (&entry, DBUS_TYPE_VARIANT, signature, &variant);
    dbus_message_iter_open_container (&variant, DBUS_TYPE_ARRAY, "s", &array);

    for (int i = 0; i < count; i++) {
	dbus_message_iter_append_basic (&array, DBUS_TYPE_STRING, &values[i]);
    }

    dbus_message_iter_close_container (&variant, &array);
    dbus_message_iter_close_container (&entry, &variant);
    dbus_message_iter_close_container (dict, &entry);
}

void appendMetadataEntries (DBusMessageIter* meta, const std::string& title) {
    DBusMessageIter metaEntry, metaVariant, artists;

    // mpris:trackid (object path)
    {
	const char* metaKey = "mpris:trackid";
	const char* signature = "o";
	const std::string trackid = MPRISServer::trackId (title);
	const char* trackidStr = trackid.c_str ();

	dbus_message_iter_open_container (meta, DBUS_TYPE_DICT_ENTRY, nullptr, &metaEntry);
	dbus_message_iter_append_basic (&metaEntry, DBUS_TYPE_STRING, &metaKey);
	dbus_message_iter_open_container (&metaEntry, DBUS_TYPE_VARIANT, signature, &metaVariant);
	dbus_message_iter_append_basic (&metaVariant, DBUS_TYPE_OBJECT_PATH, &trackidStr);
	dbus_message_iter_close_container (&metaEntry, &metaVariant);
	dbus_message_iter_close_container (meta, &metaEntry);
    }

    // xesam:title
    {
	const char* metaKey = "xesam:title";
	const char* signature = "s";
	const char* titleStr = title.c_str ();

	dbus_message_iter_open_container (meta, DBUS_TYPE_DICT_ENTRY, nullptr, &metaEntry);
	dbus_message_iter_append_basic (&metaEntry, DBUS_TYPE_STRING, &metaKey);
	dbus_message_iter_open_container (&metaEntry, DBUS_TYPE_VARIANT, signature, &metaVariant);
	dbus_message_iter_append_basic (&metaVariant, DBUS_TYPE_STRING, &titleStr);
	dbus_message_iter_close_container (&metaEntry, &metaVariant);
	dbus_message_iter_close_container (meta, &metaEntry);
    }

    // xesam:artist
    {
	const char* metaKey = "xesam:artist";
	const char* signature = "as";
	const char* artist = "linux-wallpaperengine";

	dbus_message_iter_open_container (meta, DBUS_TYPE_DICT_ENTRY, nullptr, &metaEntry);
	dbus_message_iter_append_basic (&metaEntry, DBUS_TYPE_STRING, &metaKey);
	dbus_message_iter_open_container (&metaEntry, DBUS_TYPE_VARIANT, signature, &metaVariant);
	dbus_message_iter_open_container (&metaVariant, DBUS_TYPE_ARRAY, "s", &artists);
	dbus_message_iter_append_basic (&artists, DBUS_TYPE_STRING, &artist);
	dbus_message_iter_close_container (&metaVariant, &artists);
	dbus_message_iter_close_container (&metaEntry, &metaVariant);
	dbus_message_iter_close_container (meta, &metaEntry);
    }
}

void appendMetadataValue (DBusMessageIter* dict, const std::string& title) {
    DBusMessageIter entry, variant, meta;
    const char* key = "Metadata";
    const char* metaSignature = "a{sv}";

    dbus_message_iter_open_container (dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic (&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container (&entry, DBUS_TYPE_VARIANT, metaSignature, &variant);
    dbus_message_iter_open_container (&variant, DBUS_TYPE_ARRAY, "{sv}", &meta);
    appendMetadataEntries (&meta, title);
    dbus_message_iter_close_container (&variant, &meta);
    dbus_message_iter_close_container (&entry, &variant);
    dbus_message_iter_close_container (dict, &entry);
}
} // namespace

std::string MPRISServer::trackId (const std::string& title) {
    std::string clean = "/wallpaperengine/";

    for (char c : title.empty () ? std::string ("idle") : title) {
	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
	    clean += c;
	} else {
	    clean += '_';
	}
    }

    return clean;
}

MPRISServer::MPRISServer (Callbacks callbacks) : m_callbacks (std::move (callbacks)) {
    DBusError err;
    dbus_error_init (&err);

    DBusConnection* connection = dbus_bus_get (DBUS_BUS_SESSION, &err);

    if (connection == nullptr) {
	sLog.error ("MPRIS: no session bus, player interface disabled");
	dbus_error_free (&err);
	return;
    }

    dbus_connection_set_exit_on_disconnect (connection, FALSE);

    const int nameResult = dbus_bus_request_name (
	connection, BusName, DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_ALLOW_REPLACEMENT, &err
    );

    if (nameResult != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
	sLog.error ("MPRIS: cannot own bus name, player interface disabled");
	dbus_error_free (&err);
	dbus_connection_unref (connection);
	return;
    }

    if (!dbus_connection_add_filter (connection, MPRISServer::messageHandler, this, nullptr)) {
	sLog.error ("MPRIS: cannot add filter, player interface disabled");
	dbus_connection_unref (connection);
	return;
    }

    this->m_connection = connection;
    sLog.out ("MPRIS player available as ", BusName);
}

MPRISServer::~MPRISServer () {
    if (this->m_connection != nullptr) {
	dbus_connection_remove_filter (this->m_connection, MPRISServer::messageHandler, this);
	dbus_connection_unref (this->m_connection);
	this->m_connection = nullptr;
    }
}

DBusHandlerResult MPRISServer::messageHandler (DBusConnection* connection, DBusMessage* message, void* userData) {
    auto* self = static_cast<MPRISServer*> (userData);

    if (self == nullptr) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    return self->handleMessage (connection, message);
}

DBusHandlerResult MPRISServer::handleMessage (DBusConnection* connection, DBusMessage* message) {
    if (dbus_message_get_type (message) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (strcmp (dbus_message_get_path (message), ObjectPath) != 0) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* iface = dbus_message_get_interface (message);
    const char* member = dbus_message_get_member (message);

    if (iface == nullptr || member == nullptr) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // --- introspection ---
    if (strcmp (iface, IfaceIntro) == 0 && strcmp (member, "Introspect") == 0) {
	DBusMessage* reply = dbus_message_new_method_return (message);

	if (reply == nullptr) {
	    return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}

	Data::Utils::ScopeGuard replyGuard ([reply] { dbus_message_unref (reply); });

	const char* xml = IntrospectionXml;
	dbus_message_append_args (reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
	dbus_connection_send (connection, reply, nullptr);
	return DBUS_HANDLER_RESULT_HANDLED;
    }

    // --- root methods ---
    if (strcmp (iface, IfaceRoot) == 0) {
	DBusMessage* reply = dbus_message_new_method_return (message);

	if (reply == nullptr) {
	    return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}

	Data::Utils::ScopeGuard replyGuard ([reply] { dbus_message_unref (reply); });

	if (strcmp (member, "Quit") == 0) {
	    safeCall (this->m_callbacks.quit);
	    dbus_connection_send (connection, reply, nullptr);
	    return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (strcmp (member, "Raise") == 0) {
	    dbus_connection_send (connection, reply, nullptr);
	    return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // --- player methods ---
    if (strcmp (iface, IfacePlayer) == 0) {
	DBusMessage* reply = dbus_message_new_method_return (message);

	if (reply == nullptr) {
	    return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}

	Data::Utils::ScopeGuard replyGuard ([reply] { dbus_message_unref (reply); });

	if (strcmp (member, "Next") == 0) {
	    safeCall (this->m_callbacks.next);
	} else if (strcmp (member, "Previous") == 0) {
	    safeCall (this->m_callbacks.previous);
	}
	// Play/Pause/PlayPause/Stop/Seek/SetPosition/OpenUri: acknowledged
	// without effect, engine pause is automatic (see CanPlay/CanPause).
	else if (
	    strcmp (member, "Play") != 0 && strcmp (member, "Pause") != 0 && strcmp (member, "PlayPause") != 0
	    && strcmp (member, "Stop") != 0 && strcmp (member, "Seek") != 0 && strcmp (member, "SetPosition") != 0
	    && strcmp (member, "OpenUri") != 0
	) {
	    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	dbus_connection_send (connection, reply, nullptr);
	return DBUS_HANDLER_RESULT_HANDLED;
    }

    // --- properties ---
    if (strcmp (iface, IfaceProps) == 0) {
	if (strcmp (member, "Get") == 0 || strcmp (member, "GetAll") == 0) {
	    DBusMessageIter args;
	    dbus_message_iter_init (message, &args);

	    const char* getIface = "";

	    if (dbus_message_iter_get_arg_type (&args) == DBUS_TYPE_STRING) {
		dbus_message_iter_get_basic (&args, &getIface);
	    }

	    const bool wantRoot = strcmp (getIface, IfaceRoot) == 0;
	    const bool wantPlayer = strcmp (getIface, IfacePlayer) == 0;

	    if (!wantRoot && !wantPlayer) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	    }

	    const std::string title = safeQuery<std::string> (this->m_callbacks.title, "");
	    const bool paused = safeQuery<bool> (this->m_callbacks.paused, false);
	    const bool canSkip = safeQuery<bool> (this->m_callbacks.canSkip, false);
	    const char* status = paused ? "Paused" : "Playing";

	    DBusMessage* reply = dbus_message_new_method_return (message);

	    if (reply == nullptr) {
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	    }

	    Data::Utils::ScopeGuard replyGuard ([reply] { dbus_message_unref (reply); });

	    if (strcmp (member, "GetAll") == 0) {
		DBusMessageIter dict, array;
		const char* signature = "{sv}";

		dbus_message_iter_init_append (reply, &array);
		dbus_message_iter_open_container (&array, DBUS_TYPE_ARRAY, signature, &dict);

		if (wantRoot) {
		    appendString (&dict, "Identity", "linux-wallpaperengine");
		    const char* schemes[] = { "file" };
		    appendStringArray (&dict, "SupportedUriSchemes", schemes, 1);
		    const char* mimes[] = { "video/mp4", "video/webm" };
		    appendStringArray (&dict, "SupportedMimeTypes", mimes, 2);
		    appendBool (&dict, "CanQuit", TRUE);
		    appendBool (&dict, "CanRaise", FALSE);
		    appendBool (&dict, "HasTrackList", FALSE);
		} else {
		    appendString (&dict, "PlaybackStatus", status);
		    appendMetadataValue (&dict, title);
		    appendBool (&dict, "CanGoNext", canSkip ? TRUE : FALSE);
		    appendBool (&dict, "CanGoPrevious", canSkip ? TRUE : FALSE);
		    appendBool (&dict, "CanPlay", FALSE);
		    appendBool (&dict, "CanPause", FALSE);
		    appendBool (&dict, "CanSeek", FALSE);
		    appendBool (&dict, "CanControl", TRUE);
		}

		dbus_message_iter_close_container (&array, &dict);
	    } else {
		// Get: second arg is the property name, reply is a single variant
		if (!dbus_message_iter_next (&args) || dbus_message_iter_get_arg_type (&args) != DBUS_TYPE_STRING) {
		    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		const char* prop = "";
		dbus_message_iter_get_basic (&args, &prop);

		DBusMessageIter variant;
		const char* signature = nullptr;
		std::string stringValue;
		dbus_bool_t boolValue = FALSE;
		bool isString = false;
		bool isBool = false;
		bool isMetadata = false;

		if (wantRoot) {
		    if (strcmp (prop, "Identity") == 0) {
			stringValue = "linux-wallpaperengine";
			isString = true;
		    } else if (strcmp (prop, "CanQuit") == 0) {
			boolValue = TRUE;
			isBool = true;
		    } else if (strcmp (prop, "CanRaise") == 0 || strcmp (prop, "HasTrackList") == 0) {
			boolValue = FALSE;
			isBool = true;
		    }
		} else {
		    if (strcmp (prop, "PlaybackStatus") == 0) {
			stringValue = status;
			isString = true;
		    } else if (strcmp (prop, "Metadata") == 0) {
			isMetadata = true;
		    } else if (strcmp (prop, "CanGoNext") == 0 || strcmp (prop, "CanGoPrevious") == 0) {
			boolValue = canSkip ? TRUE : FALSE;
			isBool = true;
		    } else if (strcmp (prop, "CanControl") == 0) {
			boolValue = TRUE;
			isBool = true;
		    } else if (
			strcmp (prop, "CanPlay") == 0 || strcmp (prop, "CanPause") == 0 || strcmp (prop, "CanSeek") == 0
		    ) {
			boolValue = FALSE;
			isBool = true;
		    }
		}

		if (!isString && !isBool && !isMetadata) {
		    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		dbus_message_iter_init_append (reply, &variant);

		if (isMetadata) {
		    // reply is a variant holding the a{sv} metadata dict:
		    // variant -> array -> dict entries (the array level is
		    // mandatory, appending entries straight into the variant
		    // aborts in _dbus_type_writer_recurse).
		    DBusMessageIter topVariant, metaArray, metaEntries;
		    dbus_message_iter_init_append (reply, &topVariant);
		    dbus_message_iter_open_container (&topVariant, DBUS_TYPE_VARIANT, "a{sv}", &metaArray);
		    dbus_message_iter_open_container (&metaArray, DBUS_TYPE_ARRAY, "{sv}", &metaEntries);
		    appendMetadataEntries (&metaEntries, title);
		    dbus_message_iter_close_container (&metaArray, &metaEntries);
		    dbus_message_iter_close_container (&topVariant, &metaArray);
		    dbus_connection_send (connection, reply, nullptr);
		    return DBUS_HANDLER_RESULT_HANDLED;
		}

		if (isString) {
		    signature = "s";
		    const char* value = stringValue.c_str ();
		    DBusMessageIter valueVariant;
		    dbus_message_iter_open_container (&variant, DBUS_TYPE_VARIANT, signature, &valueVariant);
		    dbus_message_iter_append_basic (&valueVariant, DBUS_TYPE_STRING, &value);
		    dbus_message_iter_close_container (&variant, &valueVariant);
		} else {
		    signature = "b";
		    DBusMessageIter valueVariant;
		    dbus_message_iter_open_container (&variant, DBUS_TYPE_VARIANT, signature, &valueVariant);
		    dbus_message_iter_append_basic (&valueVariant, DBUS_TYPE_BOOLEAN, &boolValue);
		    dbus_message_iter_close_container (&variant, &valueVariant);
		}
	    }

	    dbus_connection_send (connection, reply, nullptr);
	    return DBUS_HANDLER_RESULT_HANDLED;
	}
    }

    if (strcmp (iface, IfaceProps) == 0 && strcmp (member, "Set") == 0) {
	// read-only properties
	DBusMessage* error
	    = dbus_message_new_error (message, "org.mpris.MediaPlayer2.Error.ReadOnly", "Properties are read-only");

	if (error != nullptr) {
	    dbus_connection_send (connection, error, nullptr);
	    dbus_message_unref (error);
	}

	return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void MPRISServer::sendPropertiesChanged (DBusConnection* connection, bool title, bool status) {
    DBusMessage* signal = dbus_message_new_signal (ObjectPath, "org.freedesktop.DBus.Properties", "PropertiesChanged");

    if (signal == nullptr) {
	return;
    }

    Data::Utils::ScopeGuard signalGuard ([signal] { dbus_message_unref (signal); });

    const char* iface = IfacePlayer;
    DBusMessageIter args, changed, invalidated;

    dbus_message_iter_init_append (signal, &args);
    dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &iface);
    dbus_message_iter_open_container (&args, DBUS_TYPE_ARRAY, "{sv}", &changed);

    if (title) {
	appendMetadataValue (&changed, this->m_lastTitle);
    }

    if (status) {
	const char* statusValue = this->m_lastPaused ? "Paused" : "Playing";
	appendString (&changed, "PlaybackStatus", statusValue);
    }

    dbus_message_iter_close_container (&args, &changed);
    dbus_message_iter_open_container (&args, DBUS_TYPE_ARRAY, "s", &invalidated);
    dbus_message_iter_close_container (&args, &invalidated);

    dbus_connection_send (connection, signal, nullptr);
}

void MPRISServer::dispatch () {
    if (this->m_connection == nullptr) {
	return;
    }

    dbus_connection_read_write_dispatch (this->m_connection, 0);

    const std::string title = safeQuery<std::string> (this->m_callbacks.title, "");
    const bool paused = safeQuery<bool> (this->m_callbacks.paused, false);

    if (!this->m_haveState) {
	this->m_lastTitle = title;
	this->m_lastPaused = paused;
	this->m_haveState = true;
	return;
    }

    const bool titleChanged = title != this->m_lastTitle;
    const bool statusChanged = paused != this->m_lastPaused;

    if (titleChanged || statusChanged) {
	this->m_lastTitle = title;
	this->m_lastPaused = paused;
	this->sendPropertiesChanged (this->m_connection, titleChanged, statusChanged);
    }
}
