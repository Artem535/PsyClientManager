// Throwaway spike for issue #77 (LiveKit C++/Qt capture-and-render proof).
// Not shipped, not part of the main app build.
#include <QApplication>

#include <cstdlib>
#include <iostream>

#include "livekit/livekit.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  livekit::initialize(livekit::LogLevel::Info);
  std::cout << "LiveKit version: " << LIVEKIT_BUILD_VERSION_FULL << " ("
            << LIVEKIT_BUILD_FLAVOR << ", commit " << LIVEKIT_BUILD_COMMIT
            << ", built " << LIVEKIT_BUILD_DATE << ")" << std::endl;

  const char *url = std::getenv("LIVEKIT_URL");
  const char *token = std::getenv("LIVEKIT_TOKEN");
  if (!url || !token) {
    std::cout << "LIVEKIT_URL / LIVEKIT_TOKEN not set — room connection "
                 "will be skipped in later tasks; SDK init/shutdown still "
                 "verified below."
              << std::endl;
  }

  livekit::shutdown();
  std::cout << "livekit::initialize()/shutdown() round-trip OK." << std::endl;
  return 0;
}
