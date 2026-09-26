#include "application.h"
#include "config.h"

int main(const int argc, char *argv[]) {
  pcm::config::Config::migrate_legacy_directory();
  return pcm::Application().run(argc, argv);
}