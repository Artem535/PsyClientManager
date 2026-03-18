#pragma once

// === Qt ===
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
#include "client_charts_widget.h"
#include "qclient.h"

namespace oclero::qlementine {
class Switch;
}

namespace Ui {
class ClientInfoCard;
}

class QClientInfoCardPage final : public QWidget {
  Q_OBJECT

public:
  explicit QClientInfoCardPage(std::shared_ptr<pcm::database::Database> db,
                               QWidget *parent = nullptr);

  ~QClientInfoCardPage() override;

signals:
  void provideUpdateUI();

  void provideClearUI();

  void provideDefaultStyle();

  void provideEditModeStyle();

  void endEditMode();

  void provideSaveClient(const DuckClient &client);

public slots:
  void setClientInfo(const std::optional<DuckClient> &client);

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
  std::shared_ptr<pcm::database::Database> mDb;
  oclero::qlementine::Switch *mIsActiveSwitch = nullptr;
  ClientChartsWidget *mChartsWidget = nullptr;
  QClient mClientInfo;

  static int countAge(const QDate &birthDate);

  void connectReactiveSignals();

  void connectSignals() const;

  void setReadOnly(bool readOnly) const;
  void refreshCharts() const;
};
