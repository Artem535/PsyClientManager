#include "clientinfo.h"
#include "ui/pages/ui_clientinfo.h"
#include <objectbox.h>
#include <qdatetime.h>
#include <qpushbutton.h>
#include <qvariant.h>

ClientInfo::ClientInfo(std::shared_ptr<QClientModel> model, QWidget *parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::ClientInfo>()),
      m_client_model(model) {
  m_ui->setupUi(this);

  m_ui->listView->setModel(m_client_model.get());
  m_ui->listView->setViewMode(QListView::ViewMode::ListMode);

  change_edit_fields_mode(in_edit_mode);
  connect_widgets();

  m_ui->add_new_client_button->setVisible(in_edit_mode);
  m_ui->update_button->setVisible(in_edit_mode);
}

void ClientInfo::change_edit_fields_mode(bool edit) {
  m_ui->name_input->setEnabled(edit);
  m_ui->last_name_input->setEnabled(edit);
  m_ui->birthdate_input->setEnabled(edit);
  m_ui->age_input->setEnabled(edit);
  m_ui->add_new_client_button->setVisible(edit);
  m_ui->update_button->setVisible(edit);
}

void ClientInfo::connect_widgets() {
  connect(m_ui->listView, &QAbstractItemView::clicked, this,
          &ClientInfo::update_client_preview);
  connect(m_ui->add_update_checkbox, &QCheckBox::checkStateChanged,
          [this](const Qt::CheckState &state) {
            in_edit_mode = state == Qt::Unchecked ? false : true;
            change_edit_fields_mode(in_edit_mode);
          });
  connect(m_ui->clear_button, &QPushButton::clicked, this,
          &ClientInfo::clear_client_preview);

  connect(m_ui->update_button, &QPushButton::clicked, [this]() {
    const auto current_index = m_ui->listView->currentIndex();
    update_client_info(current_index);
  });

  connect(m_ui->add_new_client_button, &QPushButton::clicked, this,
          &ClientInfo::add_new_client);

  connect(this, &ClientInfo::end_edit, [this]() {
    in_edit_mode = false;
    change_edit_fields_mode(in_edit_mode);
  });

  connect(this, &ClientInfo::end_edit,
          [this]() { m_ui->add_update_checkbox->setChecked(false); });
}

void ClientInfo::clear_client_preview() {
  m_ui->name_input->clear();
  m_ui->last_name_input->clear();
  m_ui->birthdate_input->clear();
  m_ui->age_input->clear();
}

void ClientInfo::update_client_preview(const QModelIndex &index) {
  const auto data =
      index.data(QClientModel::ClientRoles::Full_object).value<ObxClient>();

  m_ui->name_input->setText(QString::fromStdString(data.name));
  m_ui->last_name_input->setText(QString::fromStdString(data.last_name));

  const auto birthdate = QDateTime::fromSecsSinceEpoch(data.birthday_date);
  m_ui->birthdate_input->setDate(birthdate.date());

  // Calc age from birthday
  const auto age = count_age(birthdate.date());
  m_ui->age_input->setText(QString::number(age));
}

int ClientInfo::count_age(const QDate &birthdate) const {
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

  m_client_model->setData(index, QVariant::fromValue(client));

  emit end_edit();
}

ObxClient ClientInfo::get_client_from_ui() const {
  ObxClient client;

  client.name = m_ui->name_input->text().toStdString();
  client.last_name = m_ui->last_name_input->text().toStdString();

  const auto date = m_ui->birthdate_input->date();
  client.birthday_date = QDateTime(date, QTime()).toSecsSinceEpoch();

  return client;
}

void ClientInfo::add_new_client() {
  auto client = get_client_from_ui();
  client.id = 0;
  m_client_model->add_new_client(client);

  clear_client_preview();

  emit end_edit();
}

ClientInfo::~ClientInfo() = default;
