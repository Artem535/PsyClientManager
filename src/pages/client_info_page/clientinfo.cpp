#include "clientinfo.h"
#include "ui/pages/ui_clientinfo.h"

#include <QAbstractItemView>
#include <QMessageBox>
#include <QVariant>

Q_LOGGING_CATEGORY(logClientInfo, "pcm.ClientInfo")

ClientInfo::ClientInfo(std::shared_ptr<QClientModel> model, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::ClientInfo>()),
      mClientModel(std::move(model)) {
  mUi->setupUi(this);

  mUi->listView->setModel(mClientModel.get());
  mUi->listView->setViewMode(QListView::ListMode);

  // Initialize UI
  clearUi();
  mUi->mAddNewClientButton->setVisible(false);
  mUi->mUpdateButton->setVisible(false);

  connectSignals();
  updateButtonState();
}

ClientInfo::~ClientInfo() = default;

void ClientInfo::connectSignals() {
  // Client list
  connect(mUi->listView, &QAbstractItemView::clicked, this,
          &ClientInfo::onClientSelected);

  // Edit mode toggle
  connect(mUi->mAddUpdateCheckBox, &QCheckBox::toggled, this,
          &ClientInfo::onEditModeToggled);

  // Action buttons
  connect(mUi->mUpdateButton, &QPushButton::clicked, this,
          &ClientInfo::onUpdateClicked);
  connect(mUi->mAddNewClientButton, &QPushButton::clicked, this,
          &ClientInfo::onAddClicked);
  connect(mUi->clear_button, &QPushButton::clicked, this,
          &ClientInfo::onClearClicked);

  // Birthdate change
  connect(mUi->mBirthdateInput, &QDateTimeEdit::dateChanged, this,
          &ClientInfo::onDateChanged);

  // Reactive validation
  connect(mUi->mNameInput, &QLineEdit::textChanged, this,
          &ClientInfo::updateButtonState);
  connect(mUi->mLastNameInput, &QLineEdit::textChanged, this,
          &ClientInfo::updateButtonState);
  connect(mUi->mBirthdateInput, &QDateTimeEdit::dateTimeChanged, this,
          &ClientInfo::updateButtonState);
}

void ClientInfo::onClientSelected(const QModelIndex &index) {
  if (!index.isValid()) {
    clearUi();
    return;
  }

  const auto clientVar = index.data(QClientModel::ClientRoles::Full_object);
  if (!clientVar.canConvert<ObxClient>())
    return;

  const auto client = clientVar.value<ObxClient>();
  updateUiFromClient(client);
  mEditingClient = client;
}

void ClientInfo::onEditModeToggled(const bool checked) {
  mInEditMode = checked;
  mUi->mAddNewClientButton->setVisible(checked);
  mUi->mUpdateButton->setVisible(checked);
  mUi->mNameInput->setEnabled(checked);
  mUi->mLastNameInput->setEnabled(checked);
  mUi->mBirthdateInput->setEnabled(checked);
  mUi->mAdditionalInfo->setEnabled(checked);

  if (!checked) {
    clearUi();
    mEditingClient.reset();
  }

  updateButtonState();
}

void ClientInfo::onUpdateClicked() {
  if (!mEditingClient || !validateInput())
    return;

  const auto updatedClient = getClientFromUi();
  const auto currentIndex = mUi->listView->currentIndex();

  if (mClientModel->setData(currentIndex, QVariant::fromValue(updatedClient))) {
    mEditingClient = updatedClient;
    QMessageBox::information(this, tr("Success"),
                             tr("Client updated successfully"));
    emit end_edit();
  } else {
    QMessageBox::warning(this, tr("Error"), tr("Failed to update client"));
  }
}

void ClientInfo::onAddClicked() {
  if (!validateInput())
    return;

  const auto newClient = getClientFromUi();
  mClientModel->add_new_client(newClient);
  clearUi();
  QMessageBox::information(this, tr("Success"),
                           tr("Client added successfully"));
  emit end_edit();
}

void ClientInfo::onClearClicked() {
  clearUi();
  mUi->listView->clearSelection();
  if (mInEditMode) {
    mUi->mAddUpdateCheckBox->setChecked(false);
    mInEditMode = false;
  }
}

void ClientInfo::onDateChanged(const QDate &date) {
  if (date.isValid()) {
    const int age = countAge(date);
    mUi->mAgeInput->setText(QString::number(age));
  } else {
    mUi->mAgeInput->clear();
  }
}

void ClientInfo::updateUiFromClient(const ObxClient &client) {
  mUi->mNameInput->setText(QString::fromStdString(client.name));
  mUi->mLastNameInput->setText(QString::fromStdString(client.last_name));
  mUi->mBirthdateInput->setDateTime(
      QDateTime::fromSecsSinceEpoch(client.birthday_date));
  mUi->mAdditionalInfo->setPlainText(
      QString::fromStdString(client.additional_info));
}

void ClientInfo::clearUi() {
  mUi->mNameInput->clear();
  mUi->mLastNameInput->clear();
  mUi->mBirthdateInput->clear();
  mUi->mAgeInput->clear();
  mUi->mAdditionalInfo->clear();
  mEditingClient.reset();
}

bool ClientInfo::validateInput() {
  const bool nameEmpty = mUi->mNameInput->text().trimmed().isEmpty();
  const bool lastNameEmpty = mUi->mLastNameInput->text().trimmed().isEmpty();
  const bool dateInvalid = !mUi->mBirthdateInput->date().isValid();

  if (nameEmpty) {
    QMessageBox::warning(this, tr("Error"), tr("Name cannot be empty"));
    return false;
  }
  if (lastNameEmpty) {
    QMessageBox::warning(this, tr("Error"), tr("Last name cannot be empty"));
    return false;
  }
  if (dateInvalid) {
    QMessageBox::warning(this, tr("Error"), tr("Invalid birth date"));
    return false;
  }
  return true;
}

ObxClient ClientInfo::getClientFromUi() const {
  ObxClient client;
  client.id = mEditingClient.has_value() ? mEditingClient->id : 0;
  client.name = mUi->mNameInput->text().toStdString();
  client.last_name = mUi->mLastNameInput->text().toStdString();
  client.birthday_date =
      mUi->mBirthdateInput->date().startOfDay().toSecsSinceEpoch();
  client.additional_info = mUi->mAdditionalInfo->toPlainText().toStdString();
  return client;
}

int ClientInfo::countAge(const QDate &birthDate) const {
  const QDate now = QDate::currentDate();
  int age = now.year() - birthDate.year();
  if (now.month() < birthDate.month() ||
      (now.month() == birthDate.month() && now.day() < birthDate.day())) {
    age--;
  }
  return age;
}

void ClientInfo::updateButtonState() {
  const bool hasValidName = !mUi->mNameInput->text().trimmed().isEmpty();
  const bool hasValidLastName =
      !mUi->mLastNameInput->text().trimmed().isEmpty();
  const bool hasValidDate = mUi->mBirthdateInput->date().isValid();
  const bool isValid = hasValidName && hasValidLastName && hasValidDate;

  mUi->mAddNewClientButton->setEnabled(mInEditMode && isValid);
  mUi->mUpdateButton->setEnabled(mInEditMode && isValid &&
                                 mEditingClient.has_value());
}
