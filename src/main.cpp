#include <csignal>
#include <iostream>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Library/Library.h"
#include "WallpaperEngine/Logging/Log.h"

WallpaperEngine::Application::WallpaperApplication* app;

void signalhandler (const int sig) {
    if (app == nullptr) {
	return;
    }

    // playlist control: only stores an atomic flag, the render thread
    // performs the actual (GL-unsafe) switch. Never blocks or allocates here.
    if (sig == SIGUSR1) {
	app->requestPlaylistSkip (1);
	return;
    }

    if (sig == SIGUSR2) {
	app->requestPlaylistSkip (-1);
	return;
    }

    app->signal (sig);
}

void initLogging () {
    sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
    sLog.addError (new std::ostream (std::cerr.rdbuf ()));
}

/**
 * Library catalog mode: scan/load the workshop cache and print the
 * requested listing without constructing the renderer. Returns the
 * process exit code.
 */
int runLibraryMode (WallpaperEngine::Application::ApplicationContext& appContext) {
    using WallpaperEngine::Library::Library;

    const auto& librarySettings = appContext.settings.library;

    Library library;

    if (librarySettings.refresh || !library.load ()) {
	library.scan ();

	if (!library.save ()) {
	    sLog.error ("Could not write library cache at ", Library::cachePath ().string ());
	}
    }

    std::vector<WallpaperEngine::Library::LibraryItem> results;

    if (librarySettings.search.empty () && librarySettings.typeFilter.empty ()) {
	results = library.items ();
    } else {
	// an empty query matches everything, so --type alone filters the full list
	results = library.search (librarySettings.search, librarySettings.typeFilter);
    }

    if (librarySettings.asJson) {
	std::cout << Library::toJson (results) << std::endl;
    } else {
	for (const auto& item : results) {
	    std::cout << item.id << " [" << item.type << "] " << item.title;

	    if (!item.valid) {
		std::cout << " (invalid)";
	    }

	    std::cout << std::endl;
	}
    }

    return 0;
}

int main (int argc, char* argv[]) {
    try {
	// if type parameter is specified, this is a subprocess, so no logging should be enabled from our side
	bool enableLogging = true;
	const std::string typeZygote = "--type=zygote";
	const std::string typeUtility = "--type=utility";

	for (int i = 1; i < argc; i++) {
	    if (strncmp (typeZygote.c_str (), argv[i], typeZygote.size ()) == 0) {
		enableLogging = false;
		break;
	    }

	    if (strncmp (typeUtility.c_str (), argv[i], typeUtility.size ()) == 0) {
		enableLogging = false;
		break;
	    }
	}

	if (enableLogging) {
	    initLogging ();
	}

	WallpaperEngine::Application::ApplicationContext appContext (argc, argv);

	appContext.loadSettingsFromArgv ();

	// catalog mode never starts the renderer (and needs no background)
	if (appContext.wantsLibrary ()) {
	    return runLibraryMode (appContext);
	}

	app = new WallpaperEngine::Application::WallpaperApplication (appContext);

	// halt if the list-properties option was specified
	if (appContext.settings.general.onlyListProperties) {
	    delete app;
	    return 0;
	}

	// attach signals to gracefully stop
	std::signal (SIGINT, signalhandler);
	std::signal (SIGTERM, signalhandler);
	std::signal (SIGUSR1, signalhandler);
	std::signal (SIGUSR2, signalhandler);
	std::signal (SIGKILL, signalhandler);

	// show the wallpaper application
	app->show ();

	// remove signal handlers before destroying app
	std::signal (SIGINT, SIG_DFL);
	std::signal (SIGTERM, SIG_DFL);
	std::signal (SIGUSR1, SIG_DFL);
	std::signal (SIGUSR2, SIG_DFL);
	std::signal (SIGKILL, SIG_DFL);

	delete app;

	return 0;
    } catch (const std::exception& e) {
	std::cerr << e.what () << std::endl;
	return 1;
    }
}