#include "sensitive_clipboard.h"

#include "app_settings.h"

#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QTimer>

namespace pcm::clipboard {
namespace {

QByteArray fingerprint(const QString &text) {
  return QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
}

quint64 &clipboardGeneration() {
  static quint64 generation = 0;
  return generation;
}

} // namespace

void copySensitiveText(const QString &text) {
  auto *clipboard = QApplication::clipboard();
  if (clipboard == nullptr) {
    return;
  }

  clipboard->setText(text);
  if (!pcm::app_settings::clearSensitiveClipboard()) {
    return;
  }

  const auto copiedFingerprint = fingerprint(text);
  const auto generation = ++clipboardGeneration();
  QTimer::singleShot(pcm::app_settings::sensitiveClipboardClearDelaySeconds() * 1000,
                     [copiedFingerprint, generation]() {
                       auto *currentClipboard = QApplication::clipboard();
                       if (currentClipboard == nullptr || generation != clipboardGeneration() ||
                           fingerprint(currentClipboard->text()) != copiedFingerprint) {
                         return;
                       }
                       currentClipboard->clear();
                     });
}

} // namespace pcm::clipboard
