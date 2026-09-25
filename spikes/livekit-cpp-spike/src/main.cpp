// Throwaway spike for issue #77 (LiveKit C++/Qt capture-and-render proof).
// Not shipped, not part of the main app build.
#include <QApplication>

#include <iostream>

#include "livekit/livekit.h"
#include "spike_window.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  livekit::initialize(livekit::LogLevel::Info);
  std::cout << "LiveKit version: " << LIVEKIT_BUILD_VERSION_FULL << std::endl;

  SpikeWindow window;
  window.show();

  const int result = app.exec();

  livekit::shutdown();
  return result;
}
