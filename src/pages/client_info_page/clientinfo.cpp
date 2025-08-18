#include "clientinfo.h"
#include "ui/pages/ui_clientinfo.h"
#include <objectbox.h>
#include <qdatetime.h>
#include <qpushbutton.h>
#include <qvariant.h>

ClientInfo::ClientInfo(std::shared_ptr<QClientModel> model, QWidget *parent)
    : QWidget(parent), mUI(std::make_unique<Ui::ClientInfo>()),
      mClientModel(model) {
  mUI->setupUi(this);

  mUI->listView->setModel(mClientModel.get());
  mUI->listView->setViewMode(QListView::ViewMode::ListMode);

  change_edit_fields_mode(in_edit_mode);
  connect_widgets();

  mUI->mAddNewClientButton->setVisible(in_edit_mode);
  mUI->mUpdateButton->setVisible(in_edit_mode);
}

void ClientInfo::change_edit_fields_mode(const bool enable) const {
  mUI->mNameInput->setEnabled(enable);
  mUI->mLastNameInput->setEnabled(enable);
  mUI->mBirthdateInput->setEnabled(enable);
  mUI->mAgeInput->setEnabled(enable);
  mUI->mAddNewClientButton->setVisible(enable);
  mUI->mUpdateButton->setVisible(enable);
}

void ClientInfo::connect_widgets() {
  connect(mUI->listView, &QAbstractItemView::clicked, this,
          &ClientInfo::update_client_preview);
  connect(mUI->mAddUpdateCheckBox, &QCheckBox::checkStateChanged,
          [this](const Qt::CheckState &state) {
            in_edit_mode = state != Qt::Unchecked;
            change_edit_fields_mode(in_edit_mode);
          });
  connect(mUI->clear_button, &QPushButton::clicked, this,
          &ClientInfo::clear_client_preview);

  connect(mUI->mUpdateButton, &QPushButton::clicked, [this]() {
    const auto current_index = mUI->listView->currentIndex();
    update_client_info(current_index);
  });

  connect(mUI->mAddNewClientButton, &QPushButton::clicked, this,
          &ClientInfo::add_new_client);

  connect(this, &ClientInfo::end_edit, [this]() {
    in_edit_mode = false;
    change_edit_fields_mode(in_edit_mode);
  });

  connect(this, &ClientInfo::end_edit,
          [this]() { mUI->mAddUpdateCheckBox->setChecked(false); });
  connect(mUI->mBirthdateInput, &QDateTimeEdit::dateChanged,
          [&](const QDate &date) {
            const auto countedAge = count_age(date);
            mUI->mAgeInput->setText(QString::number(countedAge));
          });
}

void ClientInfo::clear_client_preview() const {
  mUI->mNameInput->clear();
  mUI->mLastNameInput->clear();
  mUI->mBirthdateInput->clear();
  mUI->mAgeInput->clear();
}

void ClientInfo::update_client_preview(const QModelIndex &index) const {
  const auto data =
      index.data(QClientModel::ClientRoles::Full_object).value<ObxClient>();

  mUI->mNameInput->setText(QString::fromStdString(data.name));
  mUI->mLastNameInput->setText(QString::fromStdString(data.last_name));

  const auto birthdate = QDateTime::fromSecsSinceEpoch(data.birthday_date);
  mUI->mBirthdateInput->setDate(birthdate.date());

  // Calc age from birthday
  const auto age = count_age(birthdate.date());
  mUI->mAgeInput->setText(QString::number(age));

  mUI->mAdditionalInfo->setPlainText(
      QString::fromStdString(data.additional_info));
}

int ClientInfo::count_age(const QDate &birthdate) {
  const QDate current_date = QDateTime::currentDateTime().date();
  int age{current_date.year() - birthdate.year()};

  const bool is_month_less = current_date.month() < birthdate.month();

  bool is_before_birthday_day = current_date.month() < birthdate.month();
  is_before_birthday_day &= current_date.day() < birthdate.day();

  age = is_month_less || is_before_birthday_day ? age - 1 : age;
  return age;
}

void ClientInfo::update_client_info(const QModelIndex &index) {
  const auto current_client_var = index.data(QClientModel::ClientRoles::Id);
  const auto id = current_client_var.value<obx_id>();
  ObxClient client = get_client_from_ui();
  client.id = id;

  mClientModel->setData(index, QVariant::fromValue(client));

  emit end_edit();
}

ObxClient ClientInfo::get_client_from_ui() const {
  ObxClient client;

  client.name = mUI->mNameInput->text().toStdString();
  client.last_name = mUI->mLastNameInput->text().toStdString();

  const auto date = mUI->mBirthdateInput->date();
  client.birthday_date = QDateTime(date, QTime()).toSecsSinceEpoch();
  client.additional_info = mUI->mAdditionalInfo->toPlainText().toStdString();

  return client;
}

void ClientInfo::add_new_client() {
  auto client = get_client_from_ui();
  client.id = 0;
  mClientModel->add_new_client(client);

  clear_client_preview();

  emit end_edit();
}

ClientInfo::~ClientInfo() = default;
