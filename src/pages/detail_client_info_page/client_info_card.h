#pragma once

// === Qt ===
#include <QCheckBox>
#include <QDateTime>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QWidget>

// === STL ===
#include <memory>
#include <optional>

// === Local ===
#include "database.h"
#include "qclient.h"

namespace Ui {
class ClientInfoCard;
}

class QClientInfoCardPage final : public QWidget {
  Q_OBJECT

public:
  explicit QClientInfoCardPage(QWidget *parent = nullptr);

  ~QClientInfoCardPage() override;

signals:
  void provideUpdateUI();

  void provideClearUI();

  void provideDefaultStyle();

  void provideEditModeStyle();

  void endEditMode();

  void provideSaveClient(const ObxClient &client);

public slots:
  void setClientInfo(const std::optional<ObxClient> &client);

  void enterInEditMode();

  void cancelEditMode();

  void leaveEditMode();

  void updateUiProperty() const;

  void updateAgeInput(const QDate &date) const;

  void updateAvatar() const;

  void clearUI() const;

  void initDefaultStyle();

  void initEditModeStyle() const;

private:
  std::unique_ptr<Ui::ClientInfoCard> mUi;
  QClient mClientInfo;

  static int countAge(const QDate &birthDate);

  void connectReactiveSignals();

  void connectSignals() const;

  void setReadOnly(bool readOnly) const;
};
