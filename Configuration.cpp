#include "Configuration.hpp"

//
// Read me!
//
// This file defines a configuration dialog with the user. The general
// strategy is to expose agreed  configuration parameters via a custom
// interface (See  Configuration.hpp). The state exposed  through this
// public   interface  reflects   stored  or   derived  data   in  the
// Configuration::impl object.   The Configuration::impl  structure is
// an implementation of the PIMPL (a.k.a.  Cheshire Cat or compilation
// firewall) implementation hiding idiom that allows internal state to
// be completely removed from the public interface.
//
// There  is a  secondary level  of parameter  storage which  reflects
// current settings UI  state, these parameters are not  copied to the
// state   store  that   the  public   interface  exposes   until  the
// Configuration:impl::accept() operation is  successful. The accept()
// operation is  tied to the settings  OK button. The normal  and most
// convenient place to store this intermediate settings UI state is in
// the data models of the UI  controls, if that is not convenient then
// separate member variables  must be used to store that  state. It is
// important for the user experience that no publicly visible settings
// are changed  while the  settings UI are  changed i.e.  all settings
// changes   must    be   deferred   until   the    "OK"   button   is
// clicked. Conversely, all changes must  be discarded if the settings
// UI "Cancel" button is clicked.
//
// There is  a complication related  to the radio interface  since the
// this module offers  the facility to test the  radio interface. This
// test means  that the  public visibility to  the radio  being tested
// must be  changed.  To  maintain the  illusion of  deferring changes
// until they  are accepted, the  original radio related  settings are
// stored upon showing  the UI and restored if the  UI is dismissed by
// canceling.
//
// It  should be  noted that  the  settings UI  lives as  long as  the
// application client that uses it does. It is simply shown and hidden
// as it  is needed rather than  creating it on demand.  This strategy
// saves a  lot of  expensive UI  drawing at the  expense of  a little
// storage and  gives a  convenient place  to deliver  settings values
// from.
//
// Here is an overview of the high level flow of this module:
//
// 1)  On  construction the  initial  environment  is initialized  and
// initial   values  for   settings  are   read  from   the  QSettings
// database. At  this point  default values for  any new  settings are
// established by  providing a  default value  to the  QSettings value
// queries. This should be the only place where a hard coded value for
// a   settings  item   is   defined.   Any   remaining  one-time   UI
// initialization is also done. At the end of the constructor a method
// initialize_models()  is called  to  load the  UI  with the  current
// settings values.
//
// 2) When the settings UI is displayed by a client calling the exec()
// operation, only temporary state need be stored as the UI state will
// already mirror the publicly visible settings state.
//
// 3) As  the user makes  changes to  the settings UI  only validation
// need be  carried out since the  UI control data models  are used as
// the temporary store of unconfirmed settings.  As some settings will
// depend  on each  other a  validate() operation  is available,  this
// operation implements a check of any complex multi-field values.
//
// 4) If the  user discards the settings changes by  dismissing the UI
// with the  "Cancel" button;  the reject()  operation is  called. The
// reject() operation calls initialize_models()  which will revert all
// the  UI visible  state  to  the values  as  at  the initial  exec()
// operation.  No   changes  are  moved   into  the  data   fields  in
// Configuration::impl that  reflect the  settings state  published by
// the public interface (Configuration.hpp).
//
// 5) If  the user accepts the  settings changes by dismissing  the UI
// with the "OK" button; the  accept() operation is called which calls
// the validate() operation  again and, if it passes,  the fields that
// are used  to deliver  the settings  state are  updated from  the UI
// control models  or other temporary  state variables. At the  end of
// the accept()  operation, just  before hiding  the UI  and returning
// control to the caller; the new  settings values are stored into the
// settings database by a call to the write_settings() operation, thus
// ensuring that  settings changes are  saved even if  the application
// crashes or is subsequently killed.
//
// 6)  On  destruction,  which   only  happens  when  the  application
// terminates,  the settings  are saved  to the  settings database  by
// calling the  write_settings() operation. This is  largely redundant
// but is still done to save the default values of any new settings on
// an initial run.
//
// To add a new setting:
//
// 1) Update the UI with the new widget to view and change the value.
//
// 2)  Add  a member  to  Configuration::impl  to store  the  accepted
// setting state. If the setting state is dynamic; add a new signal to
// broadcast the setting value.
//
// 3) Add a  query method to the  public interface (Configuration.hpp)
// to access the  new setting value. If the settings  is dynamic; this
// step  is optional  since  value  changes will  be  broadcast via  a
// signal.
//
// 4) Add a forwarding operation to implement the new query (3) above.
//
// 5)  Add a  settings read  call to  read_settings() with  a sensible
// default value. If  the setting value is dynamic, add  a signal emit
// call to broadcast the setting value change.
//
// 6) Add  code to  initialize_models() to  load the  widget control's
// data model with the current value.
//
// 7) If there is no convenient data model field, add a data member to
// store the proposed new value. Ensure  this member has a valid value
// on exit from read_settings().
//
// 8)  Add  any  required  inter-field validation  to  the  validate()
// operation.
//
// 9) Add code to the accept()  operation to extract the setting value
// from  the  widget   control  data  model  and  load   it  into  the
// Configuration::impl  member  that  reflects  the  publicly  visible
// setting state. If  the setting value is dynamic; add  a signal emit
// call to broadcast any changed state of the setting.
//
// 10) Add  a settings  write call  to save the  setting value  to the
// settings database.
//

#include <stdexcept>
#include <iterator>
#include <algorithm>
#include <functional>
#include <limits>
#include <cmath>

#include <QApplication>
#include <QMetaType>
#include <QList>
#include <QSettings>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <QDebug>
#include <QDialog>
#include <QAction>
#include <QFileDialog>
#include <QDir>
#include <QTemporaryFile>
#include <QFormLayout>
#include <QString>
#include <QStringList>
#include <QStringListModel>
#include <QLineEdit>
#include <QRegExpValidator>
#include <QIntValidator>
#include <QThread>
#include <QTimer>
#include <QStandardPaths>
#include <QSound>
#include <QFont>
#include <QFontDialog>
#include <QColorDialog>
#include <QSerialPortInfo>
#include <QScopedPointer>
#include <QDateTimeEdit>
#include <QProcess>

#include "pimpl_impl.hpp"
#include "qt_helpers.hpp"
#include "MetaDataRegistry.hpp"
#include "SettingsGroup.hpp"
#include "FrequencyLineEdit.hpp"
#include "CandidateKeyFilter.hpp"
#include "ForeignKeyDelegate.hpp"
#include "TransceiverFactory.hpp"
#include "Transceiver.hpp"
#include "Bands.hpp"
#include "IARURegions.hpp"
#include "Modes.hpp"
#include "FrequencyList.hpp"
#include "StationList.hpp"
#include "NetworkServerLookup.hpp"
#include "MessageBox.hpp"
#include "MaidenheadLocatorValidator.hpp"
#include "CallsignValidator.hpp"

#include "varicode.h"

#include "ui_Configuration.h"
#include "moc_Configuration.cpp"

namespace
{
  // these undocumented flag values when stored in (Qt::UserRole - 1)
  // of a ComboBox item model index allow the item to be enabled or
  // disabled
  int const combo_box_item_enabled (32 | 1);
  int const combo_box_item_disabled (0);

//  QRegExp message_alphabet {"[- A-Za-z0-9+./?]*"};
  QRegExp message_alphabet {"[^\\x00-\\x1F]*"};

  // Magic numbers for file validation
  constexpr quint32 qrg_magic {0xadbccbdb};
  constexpr quint32 qrg_version {102}; // M.mm

  // Bump this versioned key every time we need to "reset" our working frequencies...
  const char * versionedFrequenciesSettingsKey = "FrequenciesForRegionModes_01";
}


//
// Dialog to get a new Frequency item
//
class FrequencyDialog final
  : public QDialog
{
public:
  using Item = FrequencyList_v2::Item;

  explicit FrequencyDialog (IARURegions * regions_model, Modes * modes_model, QWidget * parent = nullptr)
    : QDialog {parent}
  {
    setWindowTitle (QApplication::applicationName () + " - " +
                    tr ("Add Frequency"));
    region_combo_box_.setModel (regions_model);
    mode_combo_box_.setModel (modes_model);

    auto form_layout = new QFormLayout ();
    form_layout->addRow (tr ("IARU &Region:"), &region_combo_box_);
    form_layout->addRow (tr ("&Mode:"), &mode_combo_box_);
    form_layout->addRow (tr ("&Frequency (MHz):"), &frequency_line_edit_);

    auto main_layout = new QVBoxLayout (this);
    main_layout->addLayout (form_layout);

    auto button_box = new QDialogButtonBox {QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    main_layout->addWidget (button_box);

    connect (button_box, &QDialogButtonBox::accepted, this, &FrequencyDialog::accept);
    connect (button_box, &QDialogButtonBox::rejected, this, &FrequencyDialog::reject);
  }

  Item item () const
  {
    return {frequency_line_edit_.frequency ()
        , Modes::value (mode_combo_box_.currentText ())
        , IARURegions::value (region_combo_box_.currentText ())};
  }

private:
  QComboBox region_combo_box_;
  QComboBox mode_combo_box_;
  FrequencyLineEdit frequency_line_edit_;
};


//
// Dialog to get a new Station item
//
class StationDialog final
  : public QDialog
{
public:
  explicit StationDialog (StationList const * stations, Bands * bands, QWidget * parent = nullptr)
    : QDialog {parent}
    , all_bands_ {bands}
    , filtered_bands_ {new CandidateKeyFilter {bands, stations, 0, 0}}
  {
    setWindowTitle (QApplication::applicationName () + " - " + tr ("Add Schedule"));

    band_.setModel (filtered_bands_.data ());

    switch_at_.setTimeSpec(Qt::UTC);
    switch_at_.setDisplayFormat("hh:mm");

    switch_until_.setTimeSpec(Qt::UTC);
    switch_until_.setDisplayFormat("hh:mm");

    auto form_layout = new QFormLayout ();
    form_layout->addRow (tr ("&Frequency (MHz):"), &freq_);
    form_layout->addRow (tr ("&Switch at (UTC):"), &switch_at_);
    form_layout->addRow (tr ("&Until (UTC):"), &switch_until_);
    form_layout->addRow (tr ("&Description:"), &description_);

    auto main_layout = new QVBoxLayout (this);
    main_layout->addLayout (form_layout);

    auto button_box = new QDialogButtonBox {QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    main_layout->addWidget (button_box);

    connect (button_box, &QDialogButtonBox::accepted, this, &StationDialog::accept);
    connect (button_box, &QDialogButtonBox::rejected, this, &StationDialog::reject);
  }

  StationList::Station station () const
  {
    auto band = all_bands_->find(freq_.frequency());

    auto a = QDateTime(QDate(2000, 1, 1), switch_at_.time(), Qt::UTC);
    auto b = QDateTime(QDate(2000, 1, 1), switch_until_.time(), Qt::UTC);

    return {
        band,
        freq_.frequency(),
        a,
        b,
        description_.text ()};
  }

  int exec () override
  {
    filtered_bands_->set_active_key ();
    return QDialog::exec ();
  }

private:
  QScopedPointer<CandidateKeyFilter> filtered_bands_;
  Bands * all_bands_;

  QComboBox band_;
  FrequencyLineEdit freq_;
  QTimeEdit switch_at_;
  QTimeEdit switch_until_;
  QLineEdit description_;
};

class RearrangableMacrosModel
  : public QStringListModel
{
public:
  Qt::ItemFlags flags (QModelIndex const& index) const override
  {
    auto flags = QStringListModel::flags (index);
    if (index.isValid ())
      {
        // disallow drop onto existing items
        flags &= ~Qt::ItemIsDropEnabled;
      }
    return flags;
  }
};


//
// Class MessageItemDelegate
//
//	Item delegate for message entry such as free text message macros.
//
class MessageItemDelegate final
  : public QStyledItemDelegate
{
public:
  explicit MessageItemDelegate (QObject * parent = nullptr)
    : QStyledItemDelegate {parent}
  {
  }

  QWidget * createEditor (QWidget * parent
                          , QStyleOptionViewItem const& /* option*/
                          , QModelIndex const& /* index */
                          ) const override
  {
    auto editor = new QLineEdit {parent};
    editor->setFrame (false);
    editor->setValidator (new QRegExpValidator {message_alphabet, editor});
    return editor;
  }
};

// Internal implementation of the Configuration class.
class Configuration::impl final
  : public QDialog
{
  Q_OBJECT;

public:
  using FrequencyDelta = Radio::FrequencyDelta;
  using port_type = Configuration::port_type;

  explicit impl (Configuration * self, QDir const& temp_directory,
                 QSettings * settings, QWidget * parent);
  ~impl ();

  bool have_rig ();

  void transceiver_frequency (Frequency);
  void transceiver_tx_frequency (Frequency);
  void transceiver_mode (MODE);
  void transceiver_ptt (bool);
  void sync_transceiver (bool force_signal);

  Q_SLOT int exec () override;
  Q_SLOT void accept () override;
  Q_SLOT void reject () override;
  Q_SLOT void done (int) override;

private:
  typedef QList<QAudioDeviceInfo> AudioDevices;

  void read_settings ();
  void write_settings ();

  bool load_audio_devices (QAudio::Mode, QComboBox *, QAudioDeviceInfo *);
  void update_audio_channels (QComboBox const *, int, QComboBox *, bool);

  void initialize_models ();
  bool split_mode () const
  {
    return
      (WSJT_RIG_NONE_CAN_SPLIT || !rig_is_dummy_) &&
      (rig_params_.split_mode != TransceiverFactory::split_mode_none);
  }
  void set_cached_mode ();
  bool open_rig (bool force = false);
  //bool set_mode ();
  void close_rig ();
  TransceiverFactory::ParameterPack gather_rig_data ();
  void enumerate_rigs ();
  void set_rig_invariants ();
  bool validate ();
  void fill_port_combo_box (QComboBox *);
  Frequency apply_calibration (Frequency) const;
  Frequency remove_calibration (Frequency) const;

  void delete_frequencies ();
  void load_frequencies ();
  void merge_frequencies ();
  void save_frequencies ();
  void reset_frequencies ();
  void insert_frequency ();
  FrequencyList_v2::FrequencyItems read_frequencies_file (QString const&);

  void delete_stations ();
  void insert_station ();

  Q_SLOT void on_font_push_button_clicked ();
  Q_SLOT void on_tableFontButton_clicked();
  Q_SLOT void on_PTT_port_combo_box_activated (int);
  Q_SLOT void on_CAT_port_combo_box_activated (int);
  Q_SLOT void on_CAT_serial_baud_combo_box_currentIndexChanged (int);
  Q_SLOT void on_CAT_data_bits_button_group_buttonClicked (int);
  Q_SLOT void on_CAT_stop_bits_button_group_buttonClicked (int);
  Q_SLOT void on_CAT_handshake_button_group_buttonClicked (int);
  Q_SLOT void on_CAT_poll_interval_spin_box_valueChanged (int);
  Q_SLOT void on_split_mode_button_group_buttonClicked (int);
  Q_SLOT void on_test_CAT_push_button_clicked ();
  Q_SLOT void on_test_PTT_push_button_clicked (bool checked);
  Q_SLOT void on_force_DTR_combo_box_currentIndexChanged (int);
  Q_SLOT void on_force_RTS_combo_box_currentIndexChanged (int);
  Q_SLOT void on_rig_combo_box_currentIndexChanged (int);
  Q_SLOT void on_sound_input_combo_box_currentTextChanged (QString const&);
  Q_SLOT void on_sound_output_combo_box_currentTextChanged (QString const&);
  Q_SLOT void on_add_macro_push_button_clicked (bool = false);
  Q_SLOT void on_delete_macro_push_button_clicked (bool = false);
  Q_SLOT void on_PTT_method_button_group_buttonClicked (int);
  Q_SLOT void on_groups_line_edit_textChanged(QString const&);
  Q_SLOT void on_info_message_line_edit_textChanged(QString const&);
  Q_SLOT void on_cq_message_line_edit_textChanged(QString const&);
  Q_SLOT void on_reply_message_line_edit_textChanged(QString const&);
  Q_SLOT void on_add_macro_line_edit_editingFinished ();
  Q_SLOT void delete_macro ();
  void delete_selected_macros (QModelIndexList);
  Q_SLOT void on_save_path_select_push_button_clicked (bool);
  Q_SLOT void on_azel_path_select_push_button_clicked (bool);
  Q_SLOT void on_sound_cq_path_select_push_button_clicked();
  Q_SLOT void on_sound_cq_path_test_push_button_clicked();
  Q_SLOT void on_sound_cq_path_reset_push_button_clicked();
  Q_SLOT void on_sound_dm_path_select_push_button_clicked();
  Q_SLOT void on_sound_dm_path_test_push_button_clicked();
  Q_SLOT void on_sound_dm_path_reset_push_button_clicked();
  Q_SLOT void on_sound_am_path_select_push_button_clicked();
  Q_SLOT void on_sound_am_path_test_push_button_clicked();
  Q_SLOT void on_sound_am_path_reset_push_button_clicked();
  Q_SLOT void on_calibration_intercept_spin_box_valueChanged (double);
  Q_SLOT void on_calibration_slope_ppm_spin_box_valueChanged (double);
  Q_SLOT void handle_transceiver_update (TransceiverState const&, unsigned sequence_number);
  Q_SLOT void handle_transceiver_failure (QString const& reason);
  Q_SLOT void on_pbCQmsg_clicked();
  Q_SLOT void on_pbMyCall_clicked();
  Q_SLOT void on_tableBackgroundButton_clicked();
  Q_SLOT void on_tableSelectedRowBackgroundButton_clicked();
  Q_SLOT void on_tableForegroundButton_clicked();
  Q_SLOT void on_rxBackgroundButton_clicked();
  Q_SLOT void on_rxForegroundButton_clicked();
  Q_SLOT void on_rxFontButton_clicked();
  Q_SLOT void on_composeBackgroundButton_clicked();
  Q_SLOT void on_composeForegroundButton_clicked();
  Q_SLOT void on_composeFontButton_clicked();
  Q_SLOT void on_txForegroundButton_clicked();
  Q_SLOT void on_txFontButton_clicked();

  Q_SLOT void on_cbFox_clicked (bool);
  Q_SLOT void on_cbHound_clicked (bool);
  Q_SLOT void on_cbx2ToneSpacing_clicked(bool);
  Q_SLOT void on_cbx4ToneSpacing_clicked(bool);

  // typenames used as arguments must match registered type names :(
  Q_SIGNAL void start_transceiver (unsigned seqeunce_number) const;
  Q_SIGNAL void set_transceiver (Transceiver::TransceiverState const&,
                                 unsigned sequence_number) const;
  Q_SIGNAL void stop_transceiver () const;

  Configuration * const self_;	// back pointer to public interface

  QThread * transceiver_thread_;
  TransceiverFactory transceiver_factory_;
  QList<QMetaObject::Connection> rig_connections_;

  QScopedPointer<Ui::configuration_dialog> ui_;

  QSettings * settings_;

  QDir doc_dir_;
  QDir data_dir_;
  QDir temp_dir_;
  QDir writeable_data_dir_;
  QDir default_save_directory_;
  QDir save_directory_;
  QDir default_azel_directory_;
  QDir azel_directory_;

  QString sound_cq_path_; // cq message sound file
  QString sound_dm_path_; // directed message sound file
  QString sound_am_path_; // alert message sound file

  QFont font_;
  QFont next_font_;

  QFont table_font_;
  QFont next_table_font_;

  QFont rx_text_font_;
  QFont next_rx_text_font_;

  QFont tx_text_font_;
  QFont next_tx_text_font_;

  QFont compose_text_font_;
  QFont next_compose_text_font_;

  bool restart_sound_input_device_;
  bool restart_sound_output_device_;

  Type2MsgGen type_2_msg_gen_;

  QStringListModel macros_;
  RearrangableMacrosModel next_macros_;
  QAction * macro_delete_action_;

  Bands bands_;
  IARURegions regions_;
  IARURegions::Region region_;
  Modes modes_;
  FrequencyList_v2 frequencies_;
  FrequencyList_v2 next_frequencies_;
  StationList stations_;
  StationList next_stations_;

  QAction * frequency_delete_action_;
  QAction * frequency_insert_action_;
  QAction * load_frequencies_action_;
  QAction * save_frequencies_action_;
  QAction * merge_frequencies_action_;
  QAction * reset_frequencies_action_;
  FrequencyDialog * frequency_dialog_;

  QAction * station_delete_action_;
  QAction * station_insert_action_;
  StationDialog * station_dialog_;

  TransceiverFactory::ParameterPack rig_params_;
  TransceiverFactory::ParameterPack saved_rig_params_;
  TransceiverFactory::Capabilities::PortType last_port_type_;
  bool rig_is_dummy_;
  bool rig_active_;
  bool have_rig_;
  bool rig_changed_;
  TransceiverState cached_rig_state_;
  int rig_resolution_;          // see Transceiver::resolution signal
  CalibrationParams calibration_;
  bool frequency_calibration_disabled_; // not persistent
  unsigned transceiver_command_number_;
  QString dynamic_grid_;
  QString dynamic_info_;

  // configuration fields that we publish
  bool auto_switch_bands_;
  QString my_callsign_;
  QString my_grid_;
  QStringList my_groups_;
  QStringList auto_whitelist_;
  QStringList auto_blacklist_;
  QString my_info_;
  QString cq_;
  QString reply_;
  int callsign_aging_;
  int activity_aging_;
  QColor color_cq_;
  QColor next_color_cq_;
  QColor color_mycall_;
  QColor next_color_mycall_;

  QColor color_table_background_;
  QColor next_color_table_background_;
  QColor color_table_highlight_;
  QColor next_color_table_highlight_;
  QColor color_table_foreground_;
  QColor next_color_table_foreground_;

  QColor color_rx_background_;
  QColor next_color_rx_background_;
  QColor color_rx_foreground_;
  QColor next_color_rx_foreground_;
  QColor color_compose_background_;
  QColor next_color_compose_background_;
  QColor color_compose_foreground_;
  QColor next_color_compose_foreground_;
  QColor color_tx_foreground_;
  QColor next_color_tx_foreground_;
  QColor color_DXCC_;
  QColor next_color_DXCC_;
  QColor color_NewCall_;
  QColor next_color_NewCall_;
  qint32 id_interval_;
  qint32 ntrials_;
  qint32 aggressive_;
  qint32 RxBandwidth_;
  double degrade_;
  double txDelay_;
  bool id_after_73_;
  bool tx_qsy_allowed_;
  bool spot_to_reporting_networks_;
  bool transmit_directed_;
  bool autoreply_off_at_startup_;
  bool heartbeat_anywhere_;
  bool heartbeat_qso_pause_;
  bool relay_disabled_;
  bool monitor_off_at_startup_;
  bool monitor_last_used_;
  bool log_as_DATA_;
  bool report_in_comments_;
  bool prompt_to_log_;
  bool insert_blank_;
  bool DXCC_;
  bool ppfx_;
  bool clear_callsign_;
  bool miles_;
  bool avoid_allcall_;
  bool spellcheck_;
  bool quick_call_;
  bool disable_TX_on_73_;
  int heartbeat_;
  int watchdog_;
  bool TX_messages_;
  bool enable_VHF_features_;
  bool decode_at_52s_;
  bool single_decode_;
  bool twoPass_;
  bool bFox_;
  bool bHound_;
  bool x2ToneSpacing_;
  bool x4ToneSpacing_;
  bool use_dynamic_info_;
  QString opCall_;
  QString ptt_command_;
  QString aprs_server_name_;
  QString aprs_passcode_;
  port_type aprs_server_port_;

  QString udp_server_name_;
  port_type udp_server_port_;
//  QString n1mm_server_name () const;
  QString n1mm_server_name_;
  port_type n1mm_server_port_;
  bool broadcast_to_n1mm_;
//  port_type n1mm_server_port () const;
//  bool valid_n1mm_info () const;
//  bool broadcast_to_n1mm() const;
  bool accept_udp_requests_;
  bool udpWindowToFront_;
  bool udpWindowRestore_;
  bool udpEnabled_;
  DataMode data_mode_;
  bool pwrBandTxMemory_;
  bool pwrBandTuneMemory_;

  QAudioDeviceInfo audio_input_device_;
  bool default_audio_input_device_selected_;
  AudioDevice::Channel audio_input_channel_;
  QAudioDeviceInfo audio_output_device_;
  bool default_audio_output_device_selected_;
  AudioDevice::Channel audio_output_channel_;

  friend class Configuration;
};

#include "Configuration.moc"


// delegate to implementation class
Configuration::Configuration (QDir const& temp_directory,
                              QSettings * settings, QWidget * parent)
  : m_ {this, temp_directory, settings, parent}
{
}

Configuration::~Configuration ()
{
}

QDir Configuration::doc_dir () const {return m_->doc_dir_;}
QDir Configuration::data_dir () const {return m_->data_dir_;}
QDir Configuration::writeable_data_dir () const {return m_->writeable_data_dir_;}
QDir Configuration::temp_dir () const {return m_->temp_dir_;}

void Configuration::select_tab (int index) {m_->ui_->configuration_tabs->setCurrentIndex (index);}
int Configuration::exec () {return m_->exec ();}
bool Configuration::is_active () const {return m_->isVisible ();}

QAudioDeviceInfo const& Configuration::audio_input_device () const {return m_->audio_input_device_;}
AudioDevice::Channel Configuration::audio_input_channel () const {return m_->audio_input_channel_;}
QAudioDeviceInfo const& Configuration::audio_output_device () const {return m_->audio_output_device_;}
AudioDevice::Channel Configuration::audio_output_channel () const {return m_->audio_output_channel_;}
bool Configuration::restart_audio_input () const {return m_->restart_sound_input_device_;}
bool Configuration::restart_audio_output () const {return m_->restart_sound_output_device_;}
auto Configuration::type_2_msg_gen () const -> Type2MsgGen {return m_->type_2_msg_gen_;}
bool Configuration::use_dynamic_grid() const {return m_->use_dynamic_info_; }
QString Configuration::my_callsign () const {return m_->my_callsign_;}
QColor Configuration::color_table_background() const { return m_->color_table_background_; }
QColor Configuration::color_table_highlight() const  { return m_->color_table_highlight_; }
QColor Configuration::color_table_foreground() const { return m_->color_table_foreground_; }
QColor Configuration::color_CQ () const {return m_->color_cq_;}
QColor Configuration::color_MyCall () const {return m_->color_mycall_;}
QColor Configuration::color_rx_background () const {return m_->color_rx_background_;}
QColor Configuration::color_rx_foreground () const {return m_->color_rx_foreground_;}
QColor Configuration::color_tx_foreground () const {return m_->color_tx_foreground_;}
QColor Configuration::color_compose_background () const {return m_->color_compose_background_;}
QColor Configuration::color_compose_foreground () const {return m_->color_compose_foreground_;}
QColor Configuration::color_DXCC () const {return m_->color_DXCC_;}
QColor Configuration::color_NewCall () const {return m_->color_NewCall_;}
QFont Configuration::table_font () const {return m_->table_font_;}
QFont Configuration::text_font () const {return m_->font_;}
QFont Configuration::rx_text_font () const {return m_->rx_text_font_;}
QFont Configuration::tx_text_font () const {return m_->tx_text_font_;}
QFont Configuration::compose_text_font () const {return m_->compose_text_font_;}
qint32 Configuration::id_interval () const {return m_->id_interval_;}
qint32 Configuration::ntrials() const {return m_->ntrials_;}
qint32 Configuration::aggressive() const {return m_->aggressive_;}
double Configuration::degrade() const {return m_->degrade_;}
double Configuration::txDelay() const {return m_->txDelay_;}
qint32 Configuration::RxBandwidth() const {return m_->RxBandwidth_;}
bool Configuration::id_after_73 () const {return m_->id_after_73_;}
bool Configuration::tx_qsy_allowed () const {return m_->tx_qsy_allowed_;}
bool Configuration::spot_to_reporting_networks () const
{
  // rig must be open and working to spot externally
  return is_transceiver_online () && m_->spot_to_reporting_networks_;
}
void Configuration::set_spot_to_reporting_networks (bool spot)
{
    if(m_->spot_to_reporting_networks_ != spot){
        m_->spot_to_reporting_networks_ = spot;
        m_->write_settings();
    }
}

bool Configuration::transmit_directed() const { return m_->transmit_directed_; }
bool Configuration::autoreply_off_at_startup () const {return m_->autoreply_off_at_startup_;}
bool Configuration::heartbeat_anywhere() const { return m_->heartbeat_anywhere_;}
bool Configuration::heartbeat_qso_pause() const { return m_->heartbeat_qso_pause_;}
bool Configuration::relay_off() const { return m_->relay_disabled_; }
bool Configuration::monitor_off_at_startup () const {return m_->monitor_off_at_startup_;}
bool Configuration::monitor_last_used () const {return m_->rig_is_dummy_ || m_->monitor_last_used_;}
bool Configuration::log_as_DATA () const {return m_->log_as_DATA_;}
bool Configuration::report_in_comments () const {return m_->report_in_comments_;}
bool Configuration::prompt_to_log () const {return m_->prompt_to_log_;}
bool Configuration::insert_blank () const {return m_->insert_blank_;}
bool Configuration::DXCC () const {return m_->DXCC_;}
bool Configuration::ppfx() const {return m_->ppfx_;}
bool Configuration::clear_callsign () const {return m_->clear_callsign_;}
bool Configuration::miles () const {return m_->miles_;}
bool Configuration::avoid_allcall () const {return m_->avoid_allcall_;}
bool Configuration::set_avoid_allcall(bool avoid) {
    if(m_->avoid_allcall_ != avoid){
        m_->avoid_allcall_ = avoid;
        m_->write_settings();
    }
}
bool Configuration::spellcheck () const {return m_->spellcheck_;}
bool Configuration::quick_call () const {return m_->quick_call_;}
bool Configuration::disable_TX_on_73 () const {return m_->disable_TX_on_73_;}
int Configuration::heartbeat () const {return m_->heartbeat_;}
int Configuration::watchdog () const {return m_->watchdog_;}
bool Configuration::TX_messages () const {return m_->TX_messages_;}
bool Configuration::enable_VHF_features () const {return m_->enable_VHF_features_;}
bool Configuration::decode_at_52s () const {return m_->decode_at_52s_;}
bool Configuration::single_decode () const {return m_->single_decode_;}
bool Configuration::twoPass() const {return m_->twoPass_;}
bool Configuration::bFox() const {return m_->bFox_;}
bool Configuration::bHound() const {return m_->bHound_;}
bool Configuration::x2ToneSpacing() const {return m_->x2ToneSpacing_;}
bool Configuration::x4ToneSpacing() const {return m_->x4ToneSpacing_;}
bool Configuration::split_mode () const {return m_->split_mode ();}
QString Configuration::opCall() const {return m_->opCall_;}
QString Configuration::ptt_command() const { return m_->ptt_command_.trimmed();}
QString Configuration::aprs_server_name () const {return m_->aprs_server_name_;}
auto Configuration::aprs_server_port () const -> port_type {return m_->aprs_server_port_;}
QString Configuration::aprs_passcode() const { return m_->aprs_passcode_; }
QString Configuration::udp_server_name () const {return m_->udp_server_name_;}
auto Configuration::udp_server_port () const -> port_type {return m_->udp_server_port_;}
bool Configuration::accept_udp_requests () const {return m_->accept_udp_requests_;}
QString Configuration::n1mm_server_name () const {return m_->n1mm_server_name_;}
auto Configuration::n1mm_server_port () const -> port_type {return m_->n1mm_server_port_;}
bool Configuration::broadcast_to_n1mm () const {return m_->broadcast_to_n1mm_;}
bool Configuration::udpWindowToFront () const {return m_->udpWindowToFront_;}
bool Configuration::udpWindowRestore () const {return m_->udpWindowRestore_;}
bool Configuration::udpEnabled () const {return m_->udpEnabled_;}
Bands * Configuration::bands () {return &m_->bands_;}
Bands const * Configuration::bands () const {return &m_->bands_;}
StationList * Configuration::stations () {return &m_->stations_;}
StationList const * Configuration::stations () const {return &m_->stations_;}
bool Configuration::auto_switch_bands() const { return m_->auto_switch_bands_; }
IARURegions::Region Configuration::region () const {return m_->region_;}
FrequencyList_v2 * Configuration::frequencies () {return &m_->frequencies_;}
FrequencyList_v2 const * Configuration::frequencies () const {return &m_->frequencies_;}
QStringListModel * Configuration::macros () {return &m_->macros_;}
QStringListModel const * Configuration::macros () const {return &m_->macros_;}
QDir Configuration::save_directory () const {return m_->save_directory_;}
QDir Configuration::azel_directory () const {return m_->azel_directory_;}
QString Configuration::sound_cq_path() const {return m_->sound_cq_path_;}
QString Configuration::sound_dm_path() const {return m_->sound_dm_path_;}
QString Configuration::sound_am_path() const {return m_->sound_am_path_;}
QString Configuration::rig_name () const {return m_->rig_params_.rig_name;}
bool Configuration::pwrBandTxMemory () const {return m_->pwrBandTxMemory_;}
bool Configuration::pwrBandTuneMemory () const {return m_->pwrBandTuneMemory_;}

void Configuration::set_calibration (CalibrationParams params)
{
  m_->calibration_ = params;
}

void Configuration::enable_calibration (bool on)
{
  auto target_frequency = m_->remove_calibration (m_->cached_rig_state_.frequency ());
  m_->frequency_calibration_disabled_ = !on;
  transceiver_frequency (target_frequency);
}

bool Configuration::is_transceiver_online () const
{
  return m_->rig_active_;
}

bool Configuration::is_dummy_rig () const
{
  return m_->rig_is_dummy_;
}

bool Configuration::transceiver_online ()
{
#if WSJT_TRACE_CAT
  qDebug () << "Configuration::transceiver_online: " << m_->cached_rig_state_;
#endif

  return m_->have_rig ();
}

int Configuration::transceiver_resolution () const
{
  return m_->rig_resolution_;
}

void Configuration::transceiver_offline ()
{
#if WSJT_TRACE_CAT
  qDebug () << "Configuration::transceiver_offline:" << m_->cached_rig_state_;
#endif

  m_->close_rig ();
}

void Configuration::transceiver_frequency (Frequency f)
{
#if WSJT_TRACE_CAT
  qDebug () << "Configuration::transceiver_frequency:" << f << m_->cached_rig_state_;
#endif
  m_->transceiver_frequency (f);
}

void Configuration::transceiver_tx_frequency (Frequency f)
{
#if WSJT_TRACE_CAT
  qDebug () << "Configuration::transceiver_tx_frequency:" << f << m_->cached_rig_state_;
#endif

  m_->transceiver_tx_frequency (f);
}

void Configuration::transceiver_mode (MODE mode)
{
#if WSJT_TRACE_CAT
  qDebug () << "Configuration::transceiver_mode:" << mode << m_->cached_rig_state_;
#endif

  m_->transceiver_mode (mode);
}

void Configuration::transceiver_ptt (bool on)
{
#if WSJT_TRACE_CAT
  qDebug () << "Configuration::transceiver_ptt:" << on << m_->cached_rig_state_;
#endif

  m_->transceiver_ptt (on);

  auto cmd = ptt_command();
  if(!cmd.isEmpty()){
    auto p = new QProcess(this);
    if(cmd.contains("%1")){
        cmd = cmd.arg(on ? "\"on\"" : "\"off\"");
    } else {
        cmd.append(" ");
        cmd.append(on ? "\"on\"" : "\"off\"");
    }
    p->startDetached(cmd);
  }
}

void Configuration::sync_transceiver (bool force_signal, bool enforce_mode_and_split)
{
#if WSJT_TRACE_CAT
  qDebug () << "Configuration::sync_transceiver: force signal:" << force_signal << "enforce_mode_and_split:" << enforce_mode_and_split << m_->cached_rig_state_;
#endif

  m_->sync_transceiver (force_signal);
  if (!enforce_mode_and_split)
    {
      m_->transceiver_tx_frequency (0);
    }
}

bool Configuration::valid_n1mm_info () const
{
  // do very rudimentary checking on the n1mm server name and port number.
  //
  auto server_name = m_->n1mm_server_name_;
  auto port_number = m_->n1mm_server_port_;
  return(!(server_name.trimmed().isEmpty() || port_number == 0));
}

QString Configuration::my_grid() const
{
  auto grid = m_->my_grid_;
  if (m_->use_dynamic_info_ && m_->dynamic_grid_.size () >= 4) {
    grid = m_->dynamic_grid_;
  }
  return grid.trimmed();
}

QSet<QString> Configuration::my_groups() const {
    return QSet<QString>::fromList(m_->my_groups_);
}

void Configuration::addGroup(QString const &group){
    QSet<QString> groups = my_groups();
    groups.insert(group.trimmed());
    m_->my_groups_ = groups.toList();
    m_->write_settings();
}

void Configuration::removeGroup(QString const &group){
    QSet<QString> groups = my_groups();
    groups.remove(group.trimmed());
    m_->my_groups_ = groups.toList();
    m_->write_settings();
}

QSet<QString> Configuration::auto_whitelist() const {
    return QSet<QString>::fromList(m_->auto_whitelist_);
}

QSet<QString> Configuration::auto_blacklist() const {
    return QSet<QString>::fromList(m_->auto_blacklist_);
}

QString Configuration::my_info() const
{
    auto info = m_->my_info_;
    if(m_->use_dynamic_info_ && !m_->dynamic_info_.isEmpty()){
        info = m_->dynamic_info_;
    }

    return info.trimmed();
}

QString Configuration::cq_message() const
{
    return m_->cq_.trimmed();
}

QString Configuration::reply_message() const
{
    return m_->reply_.trimmed();
}

int Configuration::callsign_aging() const
{
    return m_->callsign_aging_;
}

int Configuration::activity_aging() const
{
    return m_->activity_aging_;
}

void Configuration::set_dynamic_location (QString const& grid_descriptor)
{
  m_->dynamic_grid_ = grid_descriptor.trimmed ();
}

void Configuration::set_dynamic_station_info(QString const& info)
{
  m_->dynamic_info_ = info.trimmed ();
}

namespace
{
#if defined (Q_OS_MAC)
  char const * app_root = "/../../../";
#else
  char const * app_root = "/../";
#endif
  QString doc_path ()
  {
#if CMAKE_BUILD
    if (QDir::isRelativePath (CMAKE_INSTALL_DOCDIR))
      {
	return QApplication::applicationDirPath () + app_root + CMAKE_INSTALL_DOCDIR;
      }
    return CMAKE_INSTALL_DOCDIR;
#else
    return QApplication::applicationDirPath ();
#endif
  }

  QString data_path ()
  {
#if CMAKE_BUILD
    if (QDir::isRelativePath (CMAKE_INSTALL_DATADIR))
      {
	return QApplication::applicationDirPath () + app_root + CMAKE_INSTALL_DATADIR + QChar {'/'} + CMAKE_PROJECT_NAME;
      }
    return CMAKE_INSTALL_DATADIR;
#else
    return QApplication::applicationDirPath ();
#endif
  }
}

template <typename T> void setUppercase(T* t){
    auto f = t->font();
    f.setCapitalization(QFont::AllUppercase);
    t->setFont(f);
}

Configuration::impl::impl (Configuration * self, QDir const& temp_directory,
                           QSettings * settings, QWidget * parent)
  : QDialog {parent}
  , self_ {self}
  , transceiver_thread_ {nullptr}
  , ui_ {new Ui::configuration_dialog}
  , settings_ {settings}
  , doc_dir_ {doc_path ()}
  , data_dir_ {data_path ()}
  , temp_dir_ {temp_directory}
  , writeable_data_dir_ {QStandardPaths::writableLocation (QStandardPaths::DataLocation)}
  , restart_sound_input_device_ {false}
  , restart_sound_output_device_ {false}
  , frequencies_ {&bands_}
  , next_frequencies_ {&bands_}
  , stations_ {&bands_}
  , next_stations_ {&bands_}
  , frequency_dialog_ {new FrequencyDialog {&regions_, &modes_, this}}
  , station_dialog_ {new StationDialog {&next_stations_, &bands_, this}}
  , last_port_type_ {TransceiverFactory::Capabilities::none}
  , rig_is_dummy_ {false}
  , rig_active_ {false}
  , have_rig_ {false}
  , rig_changed_ {false}
  , rig_resolution_ {0}
  , frequency_calibration_disabled_ {false}
  , transceiver_command_number_ {0}
  , degrade_ {0.}               // initialize to zero each run, not
                                // saved in settings
  , default_audio_input_device_selected_ {false}
  , default_audio_output_device_selected_ {false}
{
  ui_->setupUi (this);

//  ui_->groupBox_6->setVisible(false);              //### Temporary ??? ###

  {
    // Find a suitable data file location
    if (!writeable_data_dir_.mkpath ("."))
      {
        MessageBox::critical_message (this, tr ("Failed to create data directory"),
                                      tr ("path: \"%1\"").arg (writeable_data_dir_.absolutePath ()));
        throw std::runtime_error {"Failed to create data directory"};
      }

    // Make sure the default save directory exists
    QString save_dir {"save"};
    default_save_directory_ = writeable_data_dir_;
    default_azel_directory_ = writeable_data_dir_;
    if (!default_save_directory_.mkpath (save_dir) || !default_save_directory_.cd (save_dir))
      {
        MessageBox::critical_message (this, tr ("Failed to create save directory"),
                                      tr ("path: \"%1\%")
                                      .arg (default_save_directory_.absoluteFilePath (save_dir)));
        throw std::runtime_error {"Failed to create save directory"};
      }

    // we now have a deafult save path that exists

    // make sure samples directory exists
    QString samples_dir {"samples"};
    if (!default_save_directory_.mkpath (samples_dir))
      {
        MessageBox::critical_message (this, tr ("Failed to create samples directory"),
                                      tr ("path: \"%1\"")
                                      .arg (default_save_directory_.absoluteFilePath (samples_dir)));
        throw std::runtime_error {"Failed to create samples directory"};
      }

    QString messages_dir {"messages"};
    if (!default_save_directory_.mkpath (messages_dir))
      {
        MessageBox::critical_message (this, tr ("Failed to create messages directory"),
                                      tr ("path: \"%1\"")
                                      .arg (default_save_directory_.absoluteFilePath (messages_dir)));
        throw std::runtime_error {"Failed to create messages directory"};
      }

    // copy in any new sample files to the sample directory
    QDir dest_dir {default_save_directory_};
    dest_dir.cd (samples_dir);
    
    QDir source_dir {":/" + samples_dir};
    source_dir.cd (save_dir);
    source_dir.cd (samples_dir);
    auto list = source_dir.entryInfoList (QStringList {{"*.wav"}}, QDir::Files | QDir::Readable);
    Q_FOREACH (auto const& item, list)
      {
        if (!dest_dir.exists (item.fileName ()))
          {
            QFile file {item.absoluteFilePath ()};
            file.copy (dest_dir.absoluteFilePath (item.fileName ()));
          }
      }
  }

  // this must be done after the default paths above are set
  read_settings ();

  //
  // validation
  //
  ui_->callsign_line_edit->setValidator (new CallsignValidator {this});
  ui_->grid_line_edit->setValidator (new MaidenheadLocatorValidator {this, MaidenheadLocatorValidator::Length::doubleextended});
  ui_->add_macro_line_edit->setValidator (new QRegExpValidator {message_alphabet, this});
  ui_->info_message_line_edit->setValidator (new QRegExpValidator {message_alphabet, this});
  ui_->reply_message_line_edit->setValidator (new QRegExpValidator {message_alphabet, this});
  ui_->cq_message_line_edit->setValidator (new QRegExpValidator {message_alphabet, this});
  ui_->groups_line_edit->setValidator (new QRegExpValidator {message_alphabet, this});

  setUppercase(ui_->callsign_line_edit);
  setUppercase(ui_->grid_line_edit);
  setUppercase(ui_->add_macro_line_edit);
  setUppercase(ui_->info_message_line_edit);
  setUppercase(ui_->reply_message_line_edit);
  setUppercase(ui_->cq_message_line_edit);
  setUppercase(ui_->groups_line_edit);
  setUppercase(ui_->auto_whitelist_line_edit);

  ui_->udp_server_port_spin_box->setMinimum (1);
  ui_->udp_server_port_spin_box->setMaximum (std::numeric_limits<port_type>::max ());

  ui_->n1mm_server_port_spin_box->setMinimum (1);
  ui_->n1mm_server_port_spin_box->setMaximum (std::numeric_limits<port_type>::max ());

  //
  // assign ids to radio buttons
  //
  ui_->CAT_data_bits_button_group->setId (ui_->CAT_default_bit_radio_button, TransceiverFactory::default_data_bits);
  ui_->CAT_data_bits_button_group->setId (ui_->CAT_7_bit_radio_button, TransceiverFactory::seven_data_bits);
  ui_->CAT_data_bits_button_group->setId (ui_->CAT_8_bit_radio_button, TransceiverFactory::eight_data_bits);

  ui_->CAT_stop_bits_button_group->setId (ui_->CAT_default_stop_bit_radio_button, TransceiverFactory::default_stop_bits);
  ui_->CAT_stop_bits_button_group->setId (ui_->CAT_one_stop_bit_radio_button, TransceiverFactory::one_stop_bit);
  ui_->CAT_stop_bits_button_group->setId (ui_->CAT_two_stop_bit_radio_button, TransceiverFactory::two_stop_bits);

  ui_->CAT_handshake_button_group->setId (ui_->CAT_handshake_default_radio_button, TransceiverFactory::handshake_default);
  ui_->CAT_handshake_button_group->setId (ui_->CAT_handshake_none_radio_button, TransceiverFactory::handshake_none);
  ui_->CAT_handshake_button_group->setId (ui_->CAT_handshake_xon_radio_button, TransceiverFactory::handshake_XonXoff);
  ui_->CAT_handshake_button_group->setId (ui_->CAT_handshake_hardware_radio_button, TransceiverFactory::handshake_hardware);

  ui_->PTT_method_button_group->setId (ui_->PTT_VOX_radio_button, TransceiverFactory::PTT_method_VOX);
  ui_->PTT_method_button_group->setId (ui_->PTT_CAT_radio_button, TransceiverFactory::PTT_method_CAT);
  ui_->PTT_method_button_group->setId (ui_->PTT_DTR_radio_button, TransceiverFactory::PTT_method_DTR);
  ui_->PTT_method_button_group->setId (ui_->PTT_RTS_radio_button, TransceiverFactory::PTT_method_RTS);

  ui_->TX_audio_source_button_group->setId (ui_->TX_source_mic_radio_button, TransceiverFactory::TX_audio_source_front);
  ui_->TX_audio_source_button_group->setId (ui_->TX_source_data_radio_button, TransceiverFactory::TX_audio_source_rear);

  ui_->TX_mode_button_group->setId (ui_->mode_none_radio_button, data_mode_none);
  ui_->TX_mode_button_group->setId (ui_->mode_USB_radio_button, data_mode_USB);
  ui_->TX_mode_button_group->setId (ui_->mode_data_radio_button, data_mode_data);

  ui_->split_mode_button_group->setId (ui_->split_none_radio_button, TransceiverFactory::split_mode_none);
  ui_->split_mode_button_group->setId (ui_->split_rig_radio_button, TransceiverFactory::split_mode_rig);
  ui_->split_mode_button_group->setId (ui_->split_emulate_radio_button, TransceiverFactory::split_mode_emulate);

  //
  // setup PTT port combo box drop down content
  //
  fill_port_combo_box (ui_->PTT_port_combo_box);
  ui_->PTT_port_combo_box->addItem ("CAT");

  //
  // setup hooks to keep audio channels aligned with devices
  //
  {
    using namespace std;
    using namespace std::placeholders;

    function<void (int)> cb (bind (&Configuration::impl::update_audio_channels, this, ui_->sound_input_combo_box, _1, ui_->sound_input_channel_combo_box, false));
    connect (ui_->sound_input_combo_box, static_cast<void (QComboBox::*)(int)> (&QComboBox::currentIndexChanged), cb);
    cb = bind (&Configuration::impl::update_audio_channels, this, ui_->sound_output_combo_box, _1, ui_->sound_output_channel_combo_box, true);
    connect (ui_->sound_output_combo_box, static_cast<void (QComboBox::*)(int)> (&QComboBox::currentIndexChanged), cb);
  }

  //
  // setup macros list view
  //
  ui_->macros_list_view->setModel (&next_macros_);
  ui_->macros_list_view->setItemDelegate (new MessageItemDelegate {this});

  macro_delete_action_ = new QAction {tr ("&Delete"), ui_->macros_list_view};
  ui_->macros_list_view->insertAction (nullptr, macro_delete_action_);
  connect (macro_delete_action_, &QAction::triggered, this, &Configuration::impl::delete_macro);

  // setup IARU region combo box model
  ui_->region_combo_box->setModel (&regions_);

  //
  // setup working frequencies table model & view
  //
  frequencies_.sort (FrequencyList_v2::frequency_column);

  ui_->frequencies_table_view->setModel (&next_frequencies_);
  ui_->frequencies_table_view->sortByColumn (FrequencyList_v2::frequency_column, Qt::AscendingOrder);
  ui_->frequencies_table_view->setColumnHidden (FrequencyList_v2::frequency_mhz_column, true);

  // delegates
  auto frequencies_item_delegate = new QStyledItemDelegate {this};
  frequencies_item_delegate->setItemEditorFactory (item_editor_factory ());
  ui_->frequencies_table_view->setItemDelegate (frequencies_item_delegate);
  ui_->frequencies_table_view->setItemDelegateForColumn (FrequencyList_v2::region_column, new ForeignKeyDelegate {&regions_, 0, this});
  ui_->frequencies_table_view->setItemDelegateForColumn (FrequencyList_v2::mode_column, new ForeignKeyDelegate {&modes_, 0, this});

  // actions
  frequency_delete_action_ = new QAction {tr ("&Delete"), ui_->frequencies_table_view};
  ui_->frequencies_table_view->insertAction (nullptr, frequency_delete_action_);
  connect (frequency_delete_action_, &QAction::triggered, this, &Configuration::impl::delete_frequencies);

  frequency_insert_action_ = new QAction {tr ("&Insert ..."), ui_->frequencies_table_view};
  ui_->frequencies_table_view->insertAction (nullptr, frequency_insert_action_);
  connect (frequency_insert_action_, &QAction::triggered, this, &Configuration::impl::insert_frequency);

  load_frequencies_action_ = new QAction {tr ("&Load ..."), ui_->frequencies_table_view};
  ui_->frequencies_table_view->insertAction (nullptr, load_frequencies_action_);
  connect (load_frequencies_action_, &QAction::triggered, this, &Configuration::impl::load_frequencies);

  save_frequencies_action_ = new QAction {tr ("&Save as ..."), ui_->frequencies_table_view};
  ui_->frequencies_table_view->insertAction (nullptr, save_frequencies_action_);
  connect (save_frequencies_action_, &QAction::triggered, this, &Configuration::impl::save_frequencies);

  merge_frequencies_action_ = new QAction {tr ("&Merge ..."), ui_->frequencies_table_view};
  ui_->frequencies_table_view->insertAction (nullptr, merge_frequencies_action_);
  connect (merge_frequencies_action_, &QAction::triggered, this, &Configuration::impl::merge_frequencies);

  reset_frequencies_action_ = new QAction {tr ("&Reset"), ui_->frequencies_table_view};
  ui_->frequencies_table_view->insertAction (nullptr, reset_frequencies_action_);
  connect (reset_frequencies_action_, &QAction::triggered, this, &Configuration::impl::reset_frequencies);


  //
  // setup stations table model & view
  //
  stations_.sort (StationList::switch_at_column);

  ui_->stations_table_view->setModel (&next_stations_);
  ui_->stations_table_view->sortByColumn (StationList::switch_at_column, Qt::AscendingOrder);
  connect(ui_->auto_switch_bands_check_box, &QCheckBox::clicked, ui_->stations_table_view, &QTableView::setEnabled);

  // delegates
  auto stations_item_delegate = new QStyledItemDelegate {this};
  stations_item_delegate->setItemEditorFactory (item_editor_factory ());
  ui_->stations_table_view->setItemDelegate (stations_item_delegate);
  //ui_->stations_table_view->setItemDelegateForColumn (StationList::band_column, new ForeignKeyDelegate {&bands_, &next_stations_, 0, StationList::band_column, this});

  ui_->stations_table_view->resizeColumnToContents (StationList::band_column);
  ui_->stations_table_view->resizeColumnToContents (StationList::frequency_column);
  ui_->stations_table_view->resizeColumnToContents (StationList::switch_at_column);
  ui_->stations_table_view->resizeColumnToContents (StationList::switch_until_column);
  ui_->stations_table_view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

  // actions
  station_delete_action_ = new QAction {tr ("&Delete"), ui_->stations_table_view};
  ui_->stations_table_view->insertAction (nullptr, station_delete_action_);
  connect (station_delete_action_, &QAction::triggered, this, &Configuration::impl::delete_stations);

  station_insert_action_ = new QAction {tr ("&Insert ..."), ui_->stations_table_view};
  ui_->stations_table_view->insertAction (nullptr, station_insert_action_);
  connect (station_insert_action_, &QAction::triggered, this, &Configuration::impl::insert_station);

  //
  // load combo boxes with audio setup choices
  //
  default_audio_input_device_selected_ = load_audio_devices (QAudio::AudioInput, ui_->sound_input_combo_box, &audio_input_device_);
  default_audio_output_device_selected_ = load_audio_devices (QAudio::AudioOutput, ui_->sound_output_combo_box, &audio_output_device_);

  update_audio_channels (ui_->sound_input_combo_box, ui_->sound_input_combo_box->currentIndex (), ui_->sound_input_channel_combo_box, false);
  update_audio_channels (ui_->sound_output_combo_box, ui_->sound_output_combo_box->currentIndex (), ui_->sound_output_channel_combo_box, true);

  ui_->sound_input_channel_combo_box->setCurrentIndex (audio_input_channel_);
  ui_->sound_output_channel_combo_box->setCurrentIndex (audio_output_channel_);

  enumerate_rigs ();
  initialize_models ();

  transceiver_thread_ = new QThread {this};
  transceiver_thread_->start ();
}

Configuration::impl::~impl ()
{
  transceiver_thread_->quit ();
  transceiver_thread_->wait ();
  write_settings ();
}

void Configuration::impl::initialize_models ()
{
  auto pal = ui_->callsign_line_edit->palette ();
  if (my_callsign_.isEmpty ())
    {
      pal.setColor (QPalette::Base, "#ffccff");
    }
  else
    {
      pal.setColor (QPalette::Base, Qt::white);
    }

  ui_->callsign_line_edit->setPalette (pal);
  ui_->grid_line_edit->setPalette (pal);
  ui_->auto_switch_bands_check_box->setChecked(auto_switch_bands_);
  ui_->callsign_line_edit->setText (my_callsign_);
  ui_->grid_line_edit->setText (my_grid_.toUpper());
  ui_->callsign_aging_spin_box->setValue(callsign_aging_);
  ui_->activity_aging_spin_box->setValue(activity_aging_);
  ui_->groups_line_edit->setText(my_groups_.join(", "));
  ui_->auto_whitelist_line_edit->setText(auto_whitelist_.join(", "));
  ui_->auto_blacklist_line_edit->setText(auto_blacklist_.join(", "));
  ui_->info_message_line_edit->setText (my_info_.toUpper());
  ui_->cq_message_line_edit->setText(cq_.toUpper());
  ui_->reply_message_line_edit->setText (reply_.toUpper());
  ui_->use_dynamic_grid->setChecked(use_dynamic_info_);

  ui_->tableBackgroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_table_background_.name()).arg(next_color_table_foreground_.name()));
  ui_->tableSelectionBackgroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_table_highlight_.name()).arg(next_color_table_foreground_.name()));
  ui_->labCQ->setStyleSheet(QString("background: %1; color: %2").arg(next_color_cq_.name()).arg(next_color_table_foreground_.name()));
  ui_->labMyCall->setStyleSheet(QString("background: %1; color: %2").arg(next_color_mycall_.name()).arg(next_color_table_foreground_.name()));

  ui_->rxLabel->setStyleSheet(QString("background: %1; color: %2").arg(color_rx_background_.name()).arg(color_rx_foreground_.name()));
  ui_->rxForegroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(color_rx_background_.name()).arg(color_rx_foreground_.name()));
  ui_->txForegroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(color_rx_background_.name()).arg(color_tx_foreground_.name()));
  ui_->composeLabel->setStyleSheet(QString("background: %1; color: %2").arg(color_compose_background_.name()).arg(color_compose_foreground_.name()));

  ui_->CW_id_interval_spin_box->setValue (id_interval_);  
  ui_->sbNtrials->setValue (ntrials_);
  ui_->sbTxDelay->setValue (txDelay_);
  ui_->sbAggressive->setValue (aggressive_);
  ui_->sbDegrade->setValue (degrade_);
  ui_->sbBandwidth->setValue (RxBandwidth_);
  ui_->PTT_method_button_group->button (rig_params_.ptt_type)->setChecked (true);
  ui_->save_path_display_label->setText (save_directory_.absolutePath ());
  ui_->azel_path_display_label->setText (azel_directory_.absolutePath ());
  ui_->sound_cq_path_display_label->setText(sound_cq_path_);
  ui_->sound_dm_path_display_label->setText(sound_dm_path_);
  ui_->sound_am_path_display_label->setText(sound_am_path_);
  ui_->CW_id_after_73_check_box->setChecked (id_after_73_);
  ui_->tx_qsy_check_box->setChecked (tx_qsy_allowed_);
  ui_->psk_reporter_check_box->setChecked (spot_to_reporting_networks_);
  ui_->transmit_directed_check_box->setChecked(transmit_directed_);
  ui_->autoreply_off_check_box->setChecked (autoreply_off_at_startup_);
  ui_->heartbeat_anywhere_check_box->setChecked(heartbeat_anywhere_);
  ui_->heartbeat_qso_pause_check_box->setChecked(heartbeat_qso_pause_);
  ui_->relay_disabled_check_box->setChecked(relay_disabled_);
  ui_->monitor_off_check_box->setChecked (monitor_off_at_startup_);
  ui_->monitor_last_used_check_box->setChecked (monitor_last_used_);
  ui_->log_as_RTTY_check_box->setChecked (log_as_DATA_);
  ui_->stations_table_view->setEnabled(ui_->auto_switch_bands_check_box->isChecked());
  ui_->report_in_comments_check_box->setChecked (report_in_comments_);
  ui_->prompt_to_log_check_box->setChecked (prompt_to_log_);
  ui_->clear_callsign_check_box->setChecked (clear_callsign_);
  ui_->miles_check_box->setChecked (miles_);
  ui_->avoid_allcall_checkbox->setChecked(avoid_allcall_);
  ui_->spellcheck_check_box->setChecked(spellcheck_);
  ui_->quick_call_check_box->setChecked (quick_call_);
  ui_->disable_TX_on_73_check_box->setChecked (disable_TX_on_73_);
  ui_->heartbeat_spin_box->setValue (heartbeat_);
  ui_->tx_watchdog_spin_box->setValue (watchdog_);
  ui_->enable_VHF_features_check_box->setChecked(enable_VHF_features_);
  ui_->decode_at_52s_check_box->setChecked(decode_at_52s_);
  ui_->single_decode_check_box->setChecked(single_decode_);
  ui_->cbTwoPass->setChecked(twoPass_);
  ui_->cbFox->setChecked(bFox_);
  ui_->cbHound->setChecked(bHound_);
  ui_->cbx2ToneSpacing->setChecked(x2ToneSpacing_);
  ui_->cbx4ToneSpacing->setChecked(x4ToneSpacing_);
  ui_->type_2_msg_gen_combo_box->setCurrentIndex (type_2_msg_gen_);
  ui_->rig_combo_box->setCurrentText (rig_params_.rig_name);
  ui_->TX_mode_button_group->button (data_mode_)->setChecked (true);
  ui_->split_mode_button_group->button (rig_params_.split_mode)->setChecked (true);
  ui_->CAT_serial_baud_combo_box->setCurrentText (QString::number (rig_params_.baud));
  ui_->CAT_data_bits_button_group->button (rig_params_.data_bits)->setChecked (true);
  ui_->CAT_stop_bits_button_group->button (rig_params_.stop_bits)->setChecked (true);
  ui_->CAT_handshake_button_group->button (rig_params_.handshake)->setChecked (true);
  ui_->checkBoxPwrBandTxMemory->setChecked(pwrBandTxMemory_);
  ui_->checkBoxPwrBandTuneMemory->setChecked(pwrBandTuneMemory_);
  if (rig_params_.force_dtr)
    {
      ui_->force_DTR_combo_box->setCurrentIndex (rig_params_.dtr_high ? 1 : 2);
    }
  else
    {
      ui_->force_DTR_combo_box->setCurrentIndex (0);
    }
  if (rig_params_.force_rts)
    {
      ui_->force_RTS_combo_box->setCurrentIndex (rig_params_.rts_high ? 1 : 2);
    }
  else
    {
      ui_->force_RTS_combo_box->setCurrentIndex (0);
    }
  ui_->TX_audio_source_button_group->button (rig_params_.audio_source)->setChecked (true);
  ui_->CAT_poll_interval_spin_box->setValue (rig_params_.poll_interval);
  ui_->opCallEntry->setText (opCall_);
  ui_->ptt_command_line_edit->setText(ptt_command_);
  ui_->aprs_server_line_edit->setText (aprs_server_name_);
  ui_->aprs_server_port_spin_box->setValue (aprs_server_port_);
  ui_->aprs_passcode_line_edit->setText(aprs_passcode_);
  ui_->udp_server_line_edit->setText (udp_server_name_);
  ui_->udp_server_port_spin_box->setValue (udp_server_port_);
  ui_->accept_udp_requests_check_box->setChecked (accept_udp_requests_);
  ui_->n1mm_server_name_line_edit->setText (n1mm_server_name_);
  ui_->n1mm_server_port_spin_box->setValue (n1mm_server_port_);
  ui_->enable_n1mm_broadcast_check_box->setChecked (broadcast_to_n1mm_);
  ui_->udpWindowToFront->setChecked(udpWindowToFront_);
  ui_->udpEnable->setChecked(udpEnabled_);
  ui_->udpWindowRestore->setChecked(udpWindowRestore_);
  ui_->calibration_intercept_spin_box->setValue (calibration_.intercept);
  ui_->calibration_slope_ppm_spin_box->setValue (calibration_.slope_ppm);

  if (rig_params_.ptt_port.isEmpty ())
    {
      if (ui_->PTT_port_combo_box->count ())
        {
          ui_->PTT_port_combo_box->setCurrentText (ui_->PTT_port_combo_box->itemText (0));
        }
    }
  else
    {
      ui_->PTT_port_combo_box->setCurrentText (rig_params_.ptt_port);
    }

  ui_->region_combo_box->setCurrentIndex (region_);

  next_macros_.setStringList (macros_.stringList ());
  next_frequencies_.frequency_list (frequencies_.frequency_list ());
  next_stations_.station_list (stations_.station_list ());


  set_rig_invariants ();
}

void Configuration::impl::done (int r)
{
  // do this here since window is still on screen at this point
  SettingsGroup g {settings_, "Configuration"};
  settings_->setValue ("WindowGeometry", saveGeometry ());

  QDialog::done (r);
}

void Configuration::impl::read_settings ()
{
  SettingsGroup g {settings_, "Configuration"};
  setMinimumSize(800, 400);
  restoreGeometry (settings_->value ("WindowGeometry").toByteArray ());
  setMinimumSize(800, 400);

  auto_switch_bands_ = settings_->value("AutoSwitchBands", false).toBool();
  my_callsign_ = settings_->value ("MyCall", QString {}).toString ();
  my_grid_ = settings_->value ("MyGrid", QString {}).toString ();
  my_groups_ = settings_->value("MyGroups", QStringList{}).toStringList();
  auto_whitelist_ = settings_->value("AutoWhitelist", QStringList{}).toStringList();
  auto_blacklist_ = settings_->value("AutoBlacklist", QStringList{}).toStringList();
  callsign_aging_ = settings_->value ("CallsignAging", 0).toInt ();
  activity_aging_ = settings_->value ("ActivityAging", 2).toInt ();
  my_info_ = settings_->value("MyInfo", QString {}).toString();
  cq_ = settings_->value("CQMessage", QString {"CQCQCQ <MYGRID4>"}).toString();
  reply_ = settings_->value("Reply", QString {"HW CPY?"}).toString();
  next_color_cq_ = color_cq_ = settings_->value("colorCQ","#66ff66").toString();
  next_color_mycall_ = color_mycall_ = settings_->value("colorMyCall","#ff6666").toString();
  next_color_rx_background_ = color_rx_background_ = settings_->value("color_rx_background","#ffeaa7").toString();
  next_color_rx_foreground_ = color_rx_foreground_ = settings_->value("color_rx_foreground","#000000").toString();
  next_color_compose_background_ = color_compose_background_ = settings_->value("color_compose_background","#ffffff").toString();
  next_color_compose_foreground_ = color_compose_foreground_ = settings_->value("color_compose_foreground","#000000").toString();
  next_color_tx_foreground_ = color_tx_foreground_ = settings_->value("color_tx_foreground","#ff0000").toString();
  next_color_DXCC_ = color_DXCC_ = settings_->value("colorDXCC","#ff00ff").toString();
  next_color_NewCall_ = color_NewCall_ = settings_->value("colorNewCall","#ffaaff").toString();

  next_color_table_background_ = color_table_background_ = settings_->value("colorTableBackground", "#ffffff").toString();
  next_color_table_highlight_ = color_table_highlight_ = settings_->value("colorTableHighlight", "#3498db").toString();
  next_color_table_foreground_ = color_table_foreground_ = settings_->value("colorTableForeground","#000000").toString();

  if (next_font_.fromString (settings_->value ("Font", QGuiApplication::font ().toString ()).toString ())
      && next_font_ != font_)
    {
      font_ = next_font_;
      Q_EMIT self_->gui_text_font_changed (font_);
    }
  else
    {
      next_font_ = font_;
    }
  ui_->font_push_button->setText(QString("Application Font (%1 %2)").arg(next_font_.family()).arg(next_font_.pointSize()));

  if (next_tx_text_font_.fromString (settings_->value ("TXTextFont", QGuiApplication::font ().toString ()).toString ())
      && next_tx_text_font_ != tx_text_font_)
    {
      tx_text_font_ = next_tx_text_font_;
      Q_EMIT self_->tx_text_font_changed (tx_text_font_);
    }
  else
    {
      next_tx_text_font_ = tx_text_font_;
    }

  ui_->txFontButton->setText(QString("Font (%1 %2)").arg(next_tx_text_font_.family()).arg(next_tx_text_font_.pointSize()));

  if (next_rx_text_font_.fromString (settings_->value ("RXTextFont", QGuiApplication::font ().toString ()).toString ())
      && next_rx_text_font_ != rx_text_font_)
    {
      rx_text_font_ = next_rx_text_font_;
      Q_EMIT self_->rx_text_font_changed (rx_text_font_);
    }
  else
    {
      next_rx_text_font_ = rx_text_font_;
    }

  ui_->rxFontButton->setText(QString("Font (%1 %2)").arg(next_rx_text_font_.family()).arg(next_rx_text_font_.pointSize()));

  if (next_compose_text_font_.fromString (settings_->value ("composeTextFont", QGuiApplication::font ().toString ()).toString ())
      && next_compose_text_font_ != compose_text_font_)
    {
      compose_text_font_ = next_compose_text_font_;
      Q_EMIT self_->compose_text_font_changed (compose_text_font_);
    }
  else
    {
      next_compose_text_font_ = compose_text_font_;
    }
  ui_->composeFontButton->setText(QString("Font (%1 %2)").arg(next_compose_text_font_.family()).arg(next_compose_text_font_.pointSize()));

  if (next_table_font_.fromString (settings_->value ("tableFont", QGuiApplication::font ().toString ()).toString ())
      && next_table_font_ != table_font_)
    {
      table_font_ = next_table_font_;
      Q_EMIT self_->table_font_changed (table_font_);
    }
  else
    {
      next_table_font_ = table_font_;
    }
  ui_->tableFontButton->setText(QString("Font (%1 %2)").arg(next_table_font_.family()).arg(next_table_font_.pointSize()));

  id_interval_ = settings_->value ("IDint", 0).toInt ();
  ntrials_ = settings_->value ("nTrials", 6).toInt ();
  txDelay_ = settings_->value ("TxDelay",0.2).toDouble();
  aggressive_ = settings_->value ("Aggressive", 0).toInt ();
  RxBandwidth_ = settings_->value ("RxBandwidth", 2500).toInt ();
  save_directory_ = settings_->value ("SaveDir", default_save_directory_.absolutePath ()).toString ();
  azel_directory_ = settings_->value ("AzElDir", default_azel_directory_.absolutePath ()).toString ();
  sound_cq_path_ = settings_->value ("SoundCQPath", "").toString ();
  sound_dm_path_ = settings_->value ("SoundDMPath", "").toString ();
  sound_am_path_ = settings_->value ("SoundAMPath", "").toString ();

  {
    //
    // retrieve audio input device
    //
    auto saved_name = settings_->value ("SoundInName").toString ();

    // deal with special Windows default audio devices
    auto default_device = QAudioDeviceInfo::defaultInputDevice ();
    if (saved_name == default_device.deviceName ())
      {
        audio_input_device_ = default_device;
        default_audio_input_device_selected_ = true;
      }
    else
      {
        default_audio_input_device_selected_ = false;
        Q_FOREACH (auto const& p, QAudioDeviceInfo::availableDevices (QAudio::AudioInput)) // available audio input devices
          {
            if (p.deviceName () == saved_name)
              {
                audio_input_device_ = p;
              }
          }
      }
  }

  {
    //
    // retrieve audio output device
    //
    auto saved_name = settings_->value("SoundOutName").toString();

    // deal with special Windows default audio devices
    auto default_device = QAudioDeviceInfo::defaultOutputDevice ();
    if (saved_name == default_device.deviceName ())
      {
        audio_output_device_ = default_device;
        default_audio_output_device_selected_ = true;
      }
    else
      {
        default_audio_output_device_selected_ = false;
        Q_FOREACH (auto const& p, QAudioDeviceInfo::availableDevices (QAudio::AudioOutput)) // available audio output devices
          {
            if (p.deviceName () == saved_name)
              {
                audio_output_device_ = p;
              }
          }
      }
  }

  // retrieve audio channel info
  audio_input_channel_ = AudioDevice::fromString (settings_->value ("AudioInputChannel", "Mono").toString ());
  audio_output_channel_ = AudioDevice::fromString (settings_->value ("AudioOutputChannel", "Mono").toString ());

  type_2_msg_gen_ = settings_->value ("Type2MsgGen", QVariant::fromValue (Configuration::type_2_msg_3_full)).value<Configuration::Type2MsgGen> ();

  transmit_directed_ = settings_->value ("TransmitDirected", true).toBool();
  autoreply_off_at_startup_ = settings_->value ("AutoreplyOFF", false).toBool ();
  heartbeat_anywhere_ = settings_->value("BeaconAnywhere", false).toBool();
  heartbeat_qso_pause_ = settings_->value("HeartbeatQSOPause", true).toBool();
  relay_disabled_ = settings_->value ("RelayOFF", false).toBool ();
  monitor_off_at_startup_ = settings_->value ("MonitorOFF", false).toBool ();
  monitor_last_used_ = settings_->value ("MonitorLastUsed", false).toBool ();
  spot_to_reporting_networks_ = settings_->value ("PSKReporter", true).toBool ();
  id_after_73_ = settings_->value ("After73", false).toBool ();
  tx_qsy_allowed_ = settings_->value ("TxQSYAllowed", false).toBool ();
  use_dynamic_info_ = settings_->value ("AutoGrid", false).toBool ();

  auto loadedMacros = settings_->value ("Macros", QStringList {"TNX 73 GL"}).toStringList();

  macros_.setStringList (loadedMacros);

  region_ = settings_->value ("Region", QVariant::fromValue (IARURegions::ALL)).value<IARURegions::Region> ();

  if (settings_->contains (versionedFrequenciesSettingsKey))
    {
      auto const& v = settings_->value (versionedFrequenciesSettingsKey);
      if (v.isValid ())
        {
          frequencies_.frequency_list (v.value<FrequencyList_v2::FrequencyItems> ());
        }
      else
        {
          frequencies_.reset_to_defaults ();
        }
    }
  else
    {
      frequencies_.reset_to_defaults ();
    }

  stations_.station_list (settings_->value ("stations").value<StationList::Stations> ());

  log_as_DATA_ = settings_->value ("toRTTY", false).toBool ();
  report_in_comments_ = settings_->value("dBtoComments", false).toBool ();
  rig_params_.rig_name = settings_->value ("Rig", TransceiverFactory::basic_transceiver_name_).toString ();
  rig_is_dummy_ = TransceiverFactory::basic_transceiver_name_ == rig_params_.rig_name;
  rig_params_.network_port = settings_->value ("CATNetworkPort").toString ();
  rig_params_.usb_port = settings_->value ("CATUSBPort").toString ();
  rig_params_.serial_port = settings_->value ("CATSerialPort").toString ();
  rig_params_.baud = settings_->value ("CATSerialRate", 4800).toInt ();
  rig_params_.data_bits = settings_->value ("CATDataBits", QVariant::fromValue (TransceiverFactory::default_data_bits)).value<TransceiverFactory::DataBits> ();
  rig_params_.stop_bits = settings_->value ("CATStopBits", QVariant::fromValue (TransceiverFactory::default_stop_bits)).value<TransceiverFactory::StopBits> ();
  rig_params_.handshake = settings_->value ("CATHandshake", QVariant::fromValue (TransceiverFactory::handshake_default)).value<TransceiverFactory::Handshake> ();
  rig_params_.force_dtr = settings_->value ("CATForceDTR", false).toBool ();
  rig_params_.dtr_high = settings_->value ("DTR", false).toBool ();
  rig_params_.force_rts = settings_->value ("CATForceRTS", false).toBool ();
  rig_params_.rts_high = settings_->value ("RTS", false).toBool ();
  rig_params_.ptt_type = settings_->value ("PTTMethod", QVariant::fromValue (TransceiverFactory::PTT_method_VOX)).value<TransceiverFactory::PTTMethod> ();
  rig_params_.audio_source = settings_->value ("TXAudioSource", QVariant::fromValue (TransceiverFactory::TX_audio_source_front)).value<TransceiverFactory::TXAudioSource> ();
  rig_params_.ptt_port = settings_->value ("PTTport").toString ();
  data_mode_ = settings_->value ("DataMode", QVariant::fromValue (data_mode_none)).value<Configuration::DataMode> ();
  prompt_to_log_ = settings_->value ("PromptToLog", false).toBool ();
  insert_blank_ = settings_->value ("InsertBlank", false).toBool ();
  DXCC_ = settings_->value ("DXCCEntity", false).toBool ();
  ppfx_ = settings_->value ("PrincipalPrefix", false).toBool ();
  clear_callsign_ = settings_->value ("ClearCallGrid", false).toBool ();
  miles_ = settings_->value ("Miles", false).toBool ();
  avoid_allcall_ = settings_->value ("AvoidAllcall", false).toBool ();
  spellcheck_ = settings_->value ("Spellcheck", true).toBool();
  quick_call_ = settings_->value ("QuickCall", false).toBool ();
  disable_TX_on_73_ = settings_->value ("73TxDisable", false).toBool ();
  heartbeat_ = settings_->value ("TxBeacon", 30).toInt ();
  watchdog_ = settings_->value ("TxIdleWatchdog", 60).toInt ();
  if(watchdog_){
      watchdog_ = qMax(5, watchdog_);
  }
  TX_messages_ = settings_->value ("Tx2QSO", true).toBool ();
  enable_VHF_features_ = settings_->value("VHFUHF",false).toBool ();
  decode_at_52s_ = settings_->value("Decode52",false).toBool ();
  single_decode_ = settings_->value("SingleDecode",false).toBool ();
  twoPass_ = settings_->value("TwoPass",true).toBool ();
  bFox_ = settings_->value("Fox",false).toBool ();
  bHound_ = settings_->value("Hound",false).toBool ();
  x2ToneSpacing_ = settings_->value("x2ToneSpacing",false).toBool ();
  x4ToneSpacing_ = settings_->value("x4ToneSpacing",false).toBool ();
  rig_params_.poll_interval = settings_->value ("Polling", 0).toInt ();
  rig_params_.split_mode = settings_->value ("SplitMode", QVariant::fromValue (TransceiverFactory::split_mode_none)).value<TransceiverFactory::SplitMode> ();
  opCall_ = settings_->value ("OpCall", "").toString ();
  ptt_command_ = settings_->value("PTTCommand", "").toString();
  aprs_server_name_ = settings_->value ("aprsServer", "rotate.aprs2.net").toString ();
  aprs_server_port_ = settings_->value ("aprsServerPort", 14580).toUInt ();
  aprs_passcode_ = settings_->value ("aprsPasscode", "").toString();
  udp_server_name_ = settings_->value ("UDPServer", "127.0.0.1").toString ();
  udp_server_port_ = settings_->value ("UDPServerPort", 2237).toUInt ();
  n1mm_server_name_ = settings_->value ("N1MMServer", "127.0.0.1").toString ();
  n1mm_server_port_ = settings_->value ("N1MMServerPort", 2333).toUInt ();
  broadcast_to_n1mm_ = settings_->value ("BroadcastToN1MM", false).toBool ();
  accept_udp_requests_ = settings_->value ("AcceptUDPRequests", false).toBool ();
  udpEnabled_ = settings_->value("UDPEnabled", false).toBool();
  udpWindowToFront_ = settings_->value ("udpWindowToFront",false).toBool ();
  udpWindowRestore_ = settings_->value ("udpWindowRestore",false).toBool ();
  calibration_.intercept = settings_->value ("CalibrationIntercept", 0.).toDouble ();
  calibration_.slope_ppm = settings_->value ("CalibrationSlopePPM", 0.).toDouble ();
  pwrBandTxMemory_ = settings_->value("pwrBandTxMemory",false).toBool ();
  pwrBandTuneMemory_ = settings_->value("pwrBandTuneMemory",false).toBool ();
}

void Configuration::impl::write_settings ()
{
  SettingsGroup g {settings_, "Configuration"};

  settings_->setValue ("AutoSwitchBands", auto_switch_bands_);
  settings_->setValue ("MyCall", my_callsign_);
  settings_->setValue ("MyGrid", my_grid_);
  settings_->setValue ("MyGroups", my_groups_);
  settings_->setValue ("AutoWhitelist", auto_whitelist_);
  settings_->setValue ("AutoBlacklist", auto_blacklist_);
  settings_->setValue ("MyInfo", my_info_);
  settings_->setValue ("CQMessage", cq_);
  settings_->setValue ("Reply", reply_);
  settings_->setValue ("CallsignAging", callsign_aging_);
  settings_->setValue ("ActivityAging", activity_aging_);
  settings_->setValue("colorCQ",color_cq_);
  settings_->setValue("colorMyCall",color_mycall_);
  settings_->setValue("color_rx_background",color_rx_background_);
  settings_->setValue("color_rx_foreground",color_rx_foreground_);
  settings_->setValue("color_compose_background",color_compose_background_);
  settings_->setValue("color_compose_foreground",color_compose_foreground_);
  settings_->setValue("color_tx_foreground",color_tx_foreground_);
  settings_->setValue("colorDXCC",color_DXCC_);
  settings_->setValue("colorNewCall",color_NewCall_);

  settings_->setValue("colorTableBackground",color_table_background_);
  settings_->setValue("colorTableHighlight",color_table_highlight_);
  settings_->setValue("colorTableForeground",color_table_foreground_);


  settings_->setValue ("Font", font_.toString ());
  settings_->setValue ("RXTextFont", rx_text_font_.toString ());
  settings_->setValue ("TXTextFont", tx_text_font_.toString ());
  settings_->setValue ("composeTextFont", compose_text_font_.toString ());
  settings_->setValue ("tableFont", table_font_.toString());

  settings_->setValue ("IDint", id_interval_);
  settings_->setValue ("nTrials", ntrials_);
  settings_->setValue ("TxDelay", txDelay_);
  settings_->setValue ("Aggressive", aggressive_);
  settings_->setValue ("RxBandwidth", RxBandwidth_);
  settings_->setValue ("PTTMethod", QVariant::fromValue (rig_params_.ptt_type));
  settings_->setValue ("PTTport", rig_params_.ptt_port);
  settings_->setValue ("SaveDir", save_directory_.absolutePath ());
  settings_->setValue ("AzElDir", azel_directory_.absolutePath ());
  settings_->setValue ("SoundCQPath", sound_cq_path_);
  settings_->setValue ("SoundDMPath", sound_dm_path_);
  settings_->setValue ("SoundAMPath", sound_am_path_);

  if (default_audio_input_device_selected_)
    {
      settings_->setValue ("SoundInName", QAudioDeviceInfo::defaultInputDevice ().deviceName ());
    }
  else
    {
      settings_->setValue ("SoundInName", audio_input_device_.deviceName ());
    }

  if (default_audio_output_device_selected_)
    {
      settings_->setValue ("SoundOutName", QAudioDeviceInfo::defaultOutputDevice ().deviceName ());
    }
  else
    {
      settings_->setValue ("SoundOutName", audio_output_device_.deviceName ());
    }

  settings_->setValue ("AudioInputChannel", AudioDevice::toString (audio_input_channel_));
  settings_->setValue ("AudioOutputChannel", AudioDevice::toString (audio_output_channel_));
  settings_->setValue ("Type2MsgGen", QVariant::fromValue (type_2_msg_gen_));
  settings_->setValue ("TransmitDirected", transmit_directed_);
  settings_->setValue ("AutoreplyOFF", autoreply_off_at_startup_);
  settings_->setValue ("BeaconAnywhere", heartbeat_anywhere_);
  settings_->setValue ("HeartbeatQSOPause", heartbeat_qso_pause_);
  settings_->setValue ("RelayOFF", relay_disabled_);
  settings_->setValue ("MonitorOFF", monitor_off_at_startup_);
  settings_->setValue ("MonitorLastUsed", monitor_last_used_);
  settings_->setValue ("PSKReporter", spot_to_reporting_networks_);
  settings_->setValue ("After73", id_after_73_);
  settings_->setValue ("TxQSYAllowed", tx_qsy_allowed_);
  settings_->setValue ("Macros", macros_.stringList ());
  settings_->setValue (versionedFrequenciesSettingsKey, QVariant::fromValue (frequencies_.frequency_list ()));
  settings_->setValue ("stations", QVariant::fromValue (stations_.station_list ()));
  settings_->setValue ("toRTTY", log_as_DATA_);
  settings_->setValue ("dBtoComments", report_in_comments_);
  settings_->setValue ("Rig", rig_params_.rig_name);
  settings_->setValue ("CATNetworkPort", rig_params_.network_port);
  settings_->setValue ("CATUSBPort", rig_params_.usb_port);
  settings_->setValue ("CATSerialPort", rig_params_.serial_port);
  settings_->setValue ("CATSerialRate", rig_params_.baud);
  settings_->setValue ("CATDataBits", QVariant::fromValue (rig_params_.data_bits));
  settings_->setValue ("CATStopBits", QVariant::fromValue (rig_params_.stop_bits));
  settings_->setValue ("CATHandshake", QVariant::fromValue (rig_params_.handshake));
  settings_->setValue ("DataMode", QVariant::fromValue (data_mode_));
  settings_->setValue ("PromptToLog", prompt_to_log_);
  settings_->setValue ("InsertBlank", insert_blank_);
  settings_->setValue ("DXCCEntity", DXCC_);
  settings_->setValue ("PrincipalPrefix", ppfx_);
  settings_->setValue ("ClearCallGrid", clear_callsign_);
  settings_->setValue ("Miles", miles_);
  settings_->setValue ("AvoidAllcall", avoid_allcall_);
  settings_->setValue ("Spellcheck", spellcheck_);
  settings_->setValue ("QuickCall", quick_call_);
  settings_->setValue ("73TxDisable", disable_TX_on_73_);
  settings_->setValue ("TxBeacon", heartbeat_);
  settings_->setValue ("TxIdleWatchdog", watchdog_);
  settings_->setValue ("Tx2QSO", TX_messages_);
  settings_->setValue ("CATForceDTR", rig_params_.force_dtr);
  settings_->setValue ("DTR", rig_params_.dtr_high);
  settings_->setValue ("CATForceRTS", rig_params_.force_rts);
  settings_->setValue ("RTS", rig_params_.rts_high);
  settings_->setValue ("TXAudioSource", QVariant::fromValue (rig_params_.audio_source));
  settings_->setValue ("Polling", rig_params_.poll_interval);
  settings_->setValue ("SplitMode", QVariant::fromValue (rig_params_.split_mode));
  settings_->setValue ("VHFUHF", enable_VHF_features_);
  settings_->setValue ("Decode52", decode_at_52s_);
  settings_->setValue ("SingleDecode", single_decode_);
  settings_->setValue ("TwoPass", twoPass_);
  settings_->setValue ("Fox", bFox_);
  settings_->setValue ("Hound", bHound_);
  settings_->setValue ("x2ToneSpacing", x2ToneSpacing_);
  settings_->setValue ("x4ToneSpacing", x4ToneSpacing_);
  settings_->setValue ("OpCall", opCall_);
  settings_->setValue ("PTTCommand", ptt_command_);
  settings_->setValue ("aprsServer", aprs_server_name_);
  settings_->setValue ("aprsServerPort", aprs_server_port_);
  settings_->setValue ("aprsPasscode", aprs_passcode_);
  settings_->setValue ("UDPServer", udp_server_name_);
  settings_->setValue ("UDPServerPort", udp_server_port_);
  settings_->setValue ("N1MMServer", n1mm_server_name_);
  settings_->setValue ("N1MMServerPort", n1mm_server_port_);
  settings_->setValue ("BroadcastToN1MM", broadcast_to_n1mm_);
  settings_->setValue ("AcceptUDPRequests", accept_udp_requests_);
  settings_->setValue ("UDPEnabled", udpEnabled_);
  settings_->setValue ("udpWindowToFront", udpWindowToFront_);
  settings_->setValue ("udpWindowRestore", udpWindowRestore_);
  settings_->setValue ("CalibrationIntercept", calibration_.intercept);
  settings_->setValue ("CalibrationSlopePPM", calibration_.slope_ppm);
  settings_->setValue ("pwrBandTxMemory", pwrBandTxMemory_);
  settings_->setValue ("pwrBandTuneMemory", pwrBandTuneMemory_);
  settings_->setValue ("Region", QVariant::fromValue (region_));
  settings_->setValue ("AutoGrid", use_dynamic_info_);
}

void Configuration::impl::set_rig_invariants ()
{
  auto const& rig = ui_->rig_combo_box->currentText ();
  auto const& ptt_port = ui_->PTT_port_combo_box->currentText ();
  auto ptt_method = static_cast<TransceiverFactory::PTTMethod> (ui_->PTT_method_button_group->checkedId ());

  auto CAT_PTT_enabled = transceiver_factory_.has_CAT_PTT (rig);
  auto CAT_indirect_serial_PTT = transceiver_factory_.has_CAT_indirect_serial_PTT (rig);
  auto asynchronous_CAT = transceiver_factory_.has_asynchronous_CAT (rig);
  auto is_hw_handshake = ui_->CAT_handshake_group_box->isEnabled ()
    && TransceiverFactory::handshake_hardware == static_cast<TransceiverFactory::Handshake> (ui_->CAT_handshake_button_group->checkedId ());

  ui_->test_CAT_push_button->setStyleSheet ({});

  ui_->CAT_poll_interval_label->setEnabled (!asynchronous_CAT);
  ui_->CAT_poll_interval_spin_box->setEnabled (!asynchronous_CAT);

  auto port_type = transceiver_factory_.CAT_port_type (rig);

  bool is_serial_CAT (TransceiverFactory::Capabilities::serial == port_type);
  auto const& cat_port = ui_->CAT_port_combo_box->currentText ();

  // only enable CAT option if transceiver has CAT PTT
  ui_->PTT_CAT_radio_button->setEnabled (CAT_PTT_enabled);

  auto enable_ptt_port = TransceiverFactory::PTT_method_CAT != ptt_method && TransceiverFactory::PTT_method_VOX != ptt_method;
  ui_->PTT_port_combo_box->setEnabled (enable_ptt_port);
  // if PTT port is not enabled, fill it with the text of the CAT port
  if(!enable_ptt_port){
    ui_->PTT_port_combo_box->lineEdit()->setText(ui_->CAT_port_combo_box->currentText());
  }
  ui_->PTT_port_label->setEnabled (enable_ptt_port);

  if (CAT_indirect_serial_PTT)
    {
      ui_->PTT_port_combo_box->setItemData (ui_->PTT_port_combo_box->findText ("CAT")
                                            , combo_box_item_enabled, Qt::UserRole - 1);
    }
  else
    {
      ui_->PTT_port_combo_box->setItemData (ui_->PTT_port_combo_box->findText ("CAT")
                                            , combo_box_item_disabled, Qt::UserRole - 1);
      if ("CAT" == ui_->PTT_port_combo_box->currentText () && ui_->PTT_port_combo_box->currentIndex () > 0)
        {
          ui_->PTT_port_combo_box->setCurrentIndex (ui_->PTT_port_combo_box->currentIndex () - 1);
        }
    }
  ui_->PTT_RTS_radio_button->setEnabled (!(is_serial_CAT && ptt_port == cat_port && is_hw_handshake));

  if (TransceiverFactory::basic_transceiver_name_ == rig)
    {
      // makes no sense with rig as "None"
      ui_->monitor_last_used_check_box->setEnabled (false);

      ui_->catTab->setEnabled(false);
      //ui_->CAT_control_group_box->setEnabled (false);
      ui_->test_CAT_push_button->setEnabled (false);
      ui_->test_PTT_push_button->setEnabled (TransceiverFactory::PTT_method_DTR == ptt_method
                                             || TransceiverFactory::PTT_method_RTS == ptt_method);
      ui_->TX_audio_source_group_box->setEnabled (false);
    }
  else
    {
      ui_->monitor_last_used_check_box->setEnabled (true);
      ui_->catTab->setEnabled(true);
      //ui_->CAT_control_group_box->setEnabled (true);
      ui_->test_CAT_push_button->setEnabled (true);
      ui_->test_PTT_push_button->setEnabled (false);
      ui_->TX_audio_source_group_box->setEnabled (transceiver_factory_.has_CAT_PTT_mic_data (rig) && TransceiverFactory::PTT_method_CAT == ptt_method);
      if (port_type != last_port_type_)
        {
          last_port_type_ = port_type;
          switch (port_type)
            {
            case TransceiverFactory::Capabilities::serial:
              fill_port_combo_box (ui_->CAT_port_combo_box);
              ui_->CAT_port_combo_box->setCurrentText (rig_params_.serial_port);
              if (ui_->CAT_port_combo_box->currentText ().isEmpty () && ui_->CAT_port_combo_box->count ())
                {
                  ui_->CAT_port_combo_box->setCurrentText (ui_->CAT_port_combo_box->itemText (0));
                }
              ui_->CAT_port_label->setText (tr ("Serial Port:"));
              ui_->CAT_port_combo_box->setToolTip (tr ("Serial port used for CAT control"));
              ui_->CAT_port_combo_box->setEnabled (true);
              break;

            case TransceiverFactory::Capabilities::network:
              ui_->CAT_port_combo_box->clear ();
              ui_->CAT_port_combo_box->setCurrentText (rig_params_.network_port);
              ui_->CAT_port_label->setText (tr ("Network Server:"));
              ui_->CAT_port_combo_box->setToolTip (tr ("Optional hostname and port of network service.\n"
                                                       "Leave blank for a sensible default on this machine.\n"
                                                       "Formats:\n"
                                                       "\thostname:port\n"
                                                       "\tIPv4-address:port\n"
                                                       "\t[IPv6-address]:port"));
              ui_->CAT_port_combo_box->setEnabled (true);
              break;

            case TransceiverFactory::Capabilities::usb:
              ui_->CAT_port_combo_box->clear ();
              ui_->CAT_port_combo_box->setCurrentText (rig_params_.usb_port);
              ui_->CAT_port_label->setText (tr ("USB Device:"));
              ui_->CAT_port_combo_box->setToolTip (tr ("Optional device identification.\n"
                                                       "Leave blank for a sensible default for the rig.\n"
                                                       "Format:\n"
                                                       "\t[VID[:PID[:VENDOR[:PRODUCT]]]]"));
              ui_->CAT_port_combo_box->setEnabled (true);
              break;

            default:
              ui_->CAT_port_combo_box->clear ();
              ui_->CAT_port_combo_box->setEnabled (false);
              break;
            }
        }

      ui_->CAT_serial_port_parameters_group_box->setEnabled (is_serial_CAT);

      ui_->force_DTR_combo_box->setEnabled (is_serial_CAT
                                            && (cat_port != ptt_port
                                                || !ui_->PTT_DTR_radio_button->isEnabled ()
                                                || !ui_->PTT_DTR_radio_button->isChecked ()));
      ui_->force_RTS_combo_box->setEnabled (is_serial_CAT
                                            && !is_hw_handshake
                                            && (cat_port != ptt_port
                                                || !ui_->PTT_RTS_radio_button->isEnabled ()
                                                || !ui_->PTT_RTS_radio_button->isChecked ()));
    }
  ui_->mode_group_box->setEnabled (WSJT_RIG_NONE_CAN_SPLIT
                                   || TransceiverFactory::basic_transceiver_name_ != rig);
  ui_->split_operation_group_box->setEnabled (WSJT_RIG_NONE_CAN_SPLIT
                                              || TransceiverFactory::basic_transceiver_name_ != rig);
}


QStringList splitGroups(QString groupsString, bool filter){
    QStringList groups;
    if(groupsString.isEmpty()){
        return groups;
    }

    foreach(QString group, groupsString.split(",")){
        auto g = group.trimmed();
        if(filter && !g.startsWith("@")){
            continue;
        }
        groups.append(group.trimmed().toUpper());
    }

    return groups;
}

QStringList splitCalls(QString callsString){
    QStringList calls;
    if(callsString.isEmpty()){
        return calls;
    }

    foreach(QString call, callsString.split(",")){
        auto g = call.trimmed();
        calls.append(call.trimmed().toUpper());
    }

    return calls;
}

bool Configuration::impl::validate ()
{
  auto callsign = ui_->callsign_line_edit->text().toUpper().trimmed();
  if(!Varicode::isValidCallsign(callsign, nullptr) || callsign.startsWith("@")){
      MessageBox::critical_message (this, tr ("The callsign format you provided is not supported"));
      return false;
  }

  foreach(auto group, splitGroups(ui_->groups_line_edit->text().toUpper().trimmed(), false)){
      if(!Varicode::isCompoundCallsign(group)){
          MessageBox::critical_message (this, QString("%1 is not a valid group").arg(group));
          return false;
      }
  }

  foreach(auto call, splitCalls(ui_->auto_whitelist_line_edit->text().toUpper().trimmed())){
      if(!Varicode::isValidCallsign(call, nullptr)){
          MessageBox::critical_message (this, QString("%1 is not a valid callsign to whitelist").arg(call));
          return false;
      }
  }

  auto cq = ui_->cq_message_line_edit->text().toUpper().trimmed();
  if(!cq.isEmpty() && !(cq.startsWith("CQ") || cq.contains(callsign))){
      MessageBox::critical_message (this, QString("The CQ message format is invalid. It must either start with \"CQ\" or contain your callsign."));
      return false;
  }

  if (ui_->sound_input_combo_box->currentIndex () < 0
      && !QAudioDeviceInfo::availableDevices (QAudio::AudioInput).empty ())
    {
      MessageBox::critical_message (this, tr ("Invalid audio input device"));
      return false;
    }

  if (ui_->sound_output_combo_box->currentIndex () < 0
      && !QAudioDeviceInfo::availableDevices (QAudio::AudioOutput).empty ())
    {
      MessageBox::critical_message (this, tr ("Invalid audio out device"));
      return false;
    }

  if (!ui_->PTT_method_button_group->checkedButton ()->isEnabled ())
    {
      MessageBox::critical_message (this, tr ("Invalid PTT method"));
      return false;
    }

  auto ptt_method = static_cast<TransceiverFactory::PTTMethod> (ui_->PTT_method_button_group->checkedId ());
  auto ptt_port = ui_->PTT_port_combo_box->currentText ();
  if ((TransceiverFactory::PTT_method_DTR == ptt_method || TransceiverFactory::PTT_method_RTS == ptt_method)
      && (ptt_port.isEmpty ()
          || combo_box_item_disabled == ui_->PTT_port_combo_box->itemData (ui_->PTT_port_combo_box->findText (ptt_port), Qt::UserRole - 1)))
    {
      MessageBox::critical_message (this, tr ("Invalid PTT port"));
      return false;
    }

  return true;
}

int Configuration::impl::exec ()
{
  // macros can be modified in the main window
  next_macros_.setStringList (macros_.stringList ());

  have_rig_ = rig_active_;	// record that we started with a rig open
  saved_rig_params_ = rig_params_; // used to detect changes that
                                   // require the Transceiver to be
                                   // re-opened
  rig_changed_ = false;

  initialize_models ();
  return QDialog::exec();
}

TransceiverFactory::ParameterPack Configuration::impl::gather_rig_data ()
{
  TransceiverFactory::ParameterPack result;
  result.rig_name = ui_->rig_combo_box->currentText ();

  switch (transceiver_factory_.CAT_port_type (result.rig_name))
    {
    case TransceiverFactory::Capabilities::network:
      result.network_port = ui_->CAT_port_combo_box->currentText ();
      result.usb_port = rig_params_.usb_port;
      result.serial_port = rig_params_.serial_port;
      break;

    case TransceiverFactory::Capabilities::usb:
      result.usb_port = ui_->CAT_port_combo_box->currentText ();
      result.network_port = rig_params_.network_port;
      result.serial_port = rig_params_.serial_port;
      break;

    default:
      result.serial_port = ui_->CAT_port_combo_box->currentText ();
      result.network_port = rig_params_.network_port;
      result.usb_port = rig_params_.usb_port;
      break;
    }

  result.baud = ui_->CAT_serial_baud_combo_box->currentText ().toInt ();
  result.data_bits = static_cast<TransceiverFactory::DataBits> (ui_->CAT_data_bits_button_group->checkedId ());
  result.stop_bits = static_cast<TransceiverFactory::StopBits> (ui_->CAT_stop_bits_button_group->checkedId ());
  result.handshake = static_cast<TransceiverFactory::Handshake> (ui_->CAT_handshake_button_group->checkedId ());
  result.force_dtr = ui_->force_DTR_combo_box->isEnabled () && ui_->force_DTR_combo_box->currentIndex () > 0;
  result.dtr_high = ui_->force_DTR_combo_box->isEnabled () && 1 == ui_->force_DTR_combo_box->currentIndex ();
  result.force_rts = ui_->force_RTS_combo_box->isEnabled () && ui_->force_RTS_combo_box->currentIndex () > 0;
  result.rts_high = ui_->force_RTS_combo_box->isEnabled () && 1 == ui_->force_RTS_combo_box->currentIndex ();
  result.poll_interval = ui_->CAT_poll_interval_spin_box->value ();
  result.ptt_type = static_cast<TransceiverFactory::PTTMethod> (ui_->PTT_method_button_group->checkedId ());

  // don't allow CAT for None rig
  if(result.rig_name == "None" && result.ptt_type == TransceiverFactory::PTT_method_CAT){
      result.ptt_type = TransceiverFactory::PTT_method_VOX;
  }

  result.ptt_port = ui_->PTT_port_combo_box->currentText ();
  result.audio_source = static_cast<TransceiverFactory::TXAudioSource> (ui_->TX_audio_source_button_group->checkedId ());
  result.split_mode = static_cast<TransceiverFactory::SplitMode> (ui_->split_mode_button_group->checkedId ());
  return result;
}

void Configuration::impl::accept ()
{
  // Called when OK button is clicked.

  if (!validate ())
    {
      return;			// not accepting
    }

  // extract all rig related configuration parameters into temporary
  // structure for checking if the rig needs re-opening without
  // actually updating our live state
  auto temp_rig_params = gather_rig_data ();

  // open_rig() uses values from models so we use it to validate the
  // Transceiver settings before agreeing to accept the configuration
  if (temp_rig_params != rig_params_ && !open_rig ())
    {
      return;			// not accepting
    }

  QDialog::accept();            // do this before accessing custom
                                // models so that any changes in
                                // delegates in views get flushed to
                                // the underlying models before we
                                // access them

  sync_transceiver (true);	// force an update

  //
  // from here on we are bound to accept the new configuration
  // parameters so extract values from models and make them live
  //

  if (next_font_ != font_)
    {
      font_ = next_font_;
      Q_EMIT self_->gui_text_font_changed (font_);
    }

  if (next_tx_text_font_ != tx_text_font_)
    {
      tx_text_font_ = next_tx_text_font_;
      Q_EMIT self_->tx_text_font_changed (tx_text_font_);
    }

  if (next_rx_text_font_ != rx_text_font_)
    {
      rx_text_font_ = next_rx_text_font_;
      Q_EMIT self_->rx_text_font_changed (rx_text_font_);
    }

  if (next_compose_text_font_ != compose_text_font_)
    {
      compose_text_font_ = next_compose_text_font_;
      Q_EMIT self_->compose_text_font_changed (compose_text_font_);
    }

  if (next_table_font_ != table_font_)
    {
      table_font_ = next_table_font_;
      Q_EMIT self_->table_font_changed (table_font_);
    }

  color_cq_ = next_color_cq_;
  color_mycall_ = next_color_mycall_;
  color_rx_background_ = next_color_rx_background_;
  color_rx_foreground_ = next_color_rx_foreground_;
  color_compose_background_ = next_color_compose_background_;
  color_compose_foreground_ = next_color_compose_foreground_;
  color_tx_foreground_ = next_color_tx_foreground_;
  color_DXCC_ = next_color_DXCC_;
  color_NewCall_ = next_color_NewCall_;
  color_table_background_ = next_color_table_background_;
  color_table_highlight_ = next_color_table_highlight_;
  color_table_foreground_ = next_color_table_foreground_;

  Q_EMIT self_->colors_changed();

  rig_params_ = temp_rig_params; // now we can go live with the rig
                                 // related configuration parameters
  rig_is_dummy_ = TransceiverFactory::basic_transceiver_name_ == rig_params_.rig_name;

  // Check to see whether SoundInThread must be restarted,
  // and save user parameters.
  {
    auto const& device_name = ui_->sound_input_combo_box->currentText ();
    if (device_name != audio_input_device_.deviceName ())
      {
        auto const& default_device = QAudioDeviceInfo::defaultInputDevice ();
        if (device_name == default_device.deviceName ())
          {
            audio_input_device_ = default_device;
          }
        else
          {
            bool found {false};
            Q_FOREACH (auto const& d, QAudioDeviceInfo::availableDevices (QAudio::AudioInput))
              {
                if (device_name == d.deviceName ())
                  {
                    audio_input_device_ = d;
                    found = true;
                  }
              }
            if (!found)
              {
                audio_input_device_ = default_device;
              }
          }
        restart_sound_input_device_ = true;
      }
  }

  {
    auto const& device_name = ui_->sound_output_combo_box->currentText ();
    if (device_name != audio_output_device_.deviceName ())
      {
        auto const& default_device = QAudioDeviceInfo::defaultOutputDevice ();
        if (device_name == default_device.deviceName ())
          {
            audio_output_device_ = default_device;
          }
        else
          {
            bool found {false};
            Q_FOREACH (auto const& d, QAudioDeviceInfo::availableDevices (QAudio::AudioOutput))
              {
                if (device_name == d.deviceName ())
                  {
                    audio_output_device_ = d;
                    found = true;
                  }
              }
            if (!found)
              {
                audio_output_device_ = default_device;
              }
          }
        restart_sound_output_device_ = true;
      }
  }

  if (audio_input_channel_ != static_cast<AudioDevice::Channel> (ui_->sound_input_channel_combo_box->currentIndex ()))
    {
      audio_input_channel_ = static_cast<AudioDevice::Channel> (ui_->sound_input_channel_combo_box->currentIndex ());
      restart_sound_input_device_ = true;
    }
  Q_ASSERT (audio_input_channel_ <= AudioDevice::Right);

  if (audio_output_channel_ != static_cast<AudioDevice::Channel> (ui_->sound_output_channel_combo_box->currentIndex ()))
    {
      audio_output_channel_ = static_cast<AudioDevice::Channel> (ui_->sound_output_channel_combo_box->currentIndex ());
      restart_sound_output_device_ = true;
    }
  Q_ASSERT (audio_output_channel_ <= AudioDevice::Both);

  auto_switch_bands_ = ui_->auto_switch_bands_check_box->isChecked();
  my_callsign_ = ui_->callsign_line_edit->text ().toUpper();
  my_grid_ = ui_->grid_line_edit->text ().toUpper();
  my_groups_ = splitGroups(ui_->groups_line_edit->text().toUpper().trimmed(), true);
  auto_whitelist_ = splitCalls(ui_->auto_whitelist_line_edit->text().toUpper().trimmed());
  auto_blacklist_ = splitCalls(ui_->auto_blacklist_line_edit->text().toUpper().trimmed());
  cq_ = ui_->cq_message_line_edit->text().toUpper();
  reply_ = ui_->reply_message_line_edit->text().toUpper();
  my_info_ = ui_->info_message_line_edit->text().toUpper();
  callsign_aging_ = ui_->callsign_aging_spin_box->value();
  activity_aging_ = ui_->activity_aging_spin_box->value();
  spot_to_reporting_networks_ = ui_->psk_reporter_check_box->isChecked ();
  id_interval_ = ui_->CW_id_interval_spin_box->value ();
  ntrials_ = ui_->sbNtrials->value ();
  txDelay_ = ui_->sbTxDelay->value ();
  aggressive_ = ui_->sbAggressive->value ();
  degrade_ = ui_->sbDegrade->value ();
  RxBandwidth_ = ui_->sbBandwidth->value ();
  id_after_73_ = ui_->CW_id_after_73_check_box->isChecked ();
  tx_qsy_allowed_ = ui_->tx_qsy_check_box->isChecked ();
  transmit_directed_ = ui_->transmit_directed_check_box->isChecked();
  autoreply_off_at_startup_ = ui_->autoreply_off_check_box->isChecked ();
  heartbeat_anywhere_ = ui_->heartbeat_anywhere_check_box->isChecked();
  heartbeat_qso_pause_ = ui_->heartbeat_qso_pause_check_box->isChecked();
  relay_disabled_ = ui_->relay_disabled_check_box->isChecked();
  monitor_off_at_startup_ = ui_->monitor_off_check_box->isChecked ();
  monitor_last_used_ = ui_->monitor_last_used_check_box->isChecked ();
  type_2_msg_gen_ = static_cast<Type2MsgGen> (ui_->type_2_msg_gen_combo_box->currentIndex ());
  log_as_DATA_ = ui_->log_as_RTTY_check_box->isChecked ();
  report_in_comments_ = ui_->report_in_comments_check_box->isChecked ();
  prompt_to_log_ = ui_->prompt_to_log_check_box->isChecked ();
  clear_callsign_ = ui_->clear_callsign_check_box->isChecked ();
  miles_ = ui_->miles_check_box->isChecked ();
  avoid_allcall_ = ui_->avoid_allcall_checkbox->isChecked();
  spellcheck_ = ui_->spellcheck_check_box->isChecked();
  quick_call_ = ui_->quick_call_check_box->isChecked ();
  disable_TX_on_73_ = ui_->disable_TX_on_73_check_box->isChecked ();
  heartbeat_ = ui_->heartbeat_spin_box->value ();
  watchdog_ = ui_->tx_watchdog_spin_box->value ();
  data_mode_ = static_cast<DataMode> (ui_->TX_mode_button_group->checkedId ());
  save_directory_ = ui_->save_path_display_label->text ();
  azel_directory_ = ui_->azel_path_display_label->text ();
  sound_cq_path_ = ui_->sound_cq_path_display_label->text();
  sound_dm_path_ = ui_->sound_dm_path_display_label->text();
  sound_am_path_ = ui_->sound_am_path_display_label->text();
  enable_VHF_features_ = ui_->enable_VHF_features_check_box->isChecked ();
  decode_at_52s_ = ui_->decode_at_52s_check_box->isChecked ();
  single_decode_ = ui_->single_decode_check_box->isChecked ();
  twoPass_ = ui_->cbTwoPass->isChecked ();
  bFox_ = ui_->cbFox->isChecked ();
  bHound_ = ui_->cbHound->isChecked ();
  x2ToneSpacing_ = ui_->cbx2ToneSpacing->isChecked ();
  x4ToneSpacing_ = ui_->cbx4ToneSpacing->isChecked ();
  calibration_.intercept = ui_->calibration_intercept_spin_box->value ();
  calibration_.slope_ppm = ui_->calibration_slope_ppm_spin_box->value ();
  pwrBandTxMemory_ = ui_->checkBoxPwrBandTxMemory->isChecked ();
  pwrBandTuneMemory_ = ui_->checkBoxPwrBandTuneMemory->isChecked ();
  opCall_=ui_->opCallEntry->text();
  ptt_command_ = ui_->ptt_command_line_edit->text();
  aprs_server_name_ = ui_->aprs_server_line_edit->text();
  aprs_server_port_ = ui_->aprs_server_port_spin_box->value();
  aprs_passcode_ = ui_->aprs_passcode_line_edit->text();

  auto newUdpEnabled = ui_->udpEnable->isChecked();
  auto new_server = ui_->udp_server_line_edit->text ();
  if (new_server != udp_server_name_ || newUdpEnabled != udpEnabled_)
    {
      udp_server_name_ = new_server;
      udpEnabled_ = newUdpEnabled;

      Q_EMIT self_->udp_server_changed (udpEnabled_ ? new_server : "");
    }

  auto new_port = ui_->udp_server_port_spin_box->value ();
  if (new_port != udp_server_port_)
    {
      udp_server_port_ = new_port;
      Q_EMIT self_->udp_server_port_changed (new_port);
    }

  accept_udp_requests_ = ui_->accept_udp_requests_check_box->isChecked ();
  auto new_n1mm_server = ui_->n1mm_server_name_line_edit->text ();
  n1mm_server_name_ = new_n1mm_server;
  auto new_n1mm_port = ui_->n1mm_server_port_spin_box->value ();
  n1mm_server_port_ = new_n1mm_port;
  broadcast_to_n1mm_ = ui_->enable_n1mm_broadcast_check_box->isChecked ();

  udpWindowToFront_ = ui_->udpWindowToFront->isChecked ();
  udpWindowRestore_ = ui_->udpWindowRestore->isChecked ();


  if (macros_.stringList () != next_macros_.stringList ())
    {
      macros_.setStringList (next_macros_.stringList ());
    }

  region_ = IARURegions::value (ui_->region_combo_box->currentText ());

  if (frequencies_.frequency_list () != next_frequencies_.frequency_list ())
    {
      frequencies_.frequency_list (next_frequencies_.frequency_list ());
      frequencies_.sort (FrequencyList_v2::frequency_column);
    }

  if (stations_.station_list () != next_stations_.station_list ())
    {
      stations_.station_list(next_stations_.station_list ());
      stations_.sort (StationList::switch_at_column);

      Q_EMIT self_->band_schedule_changed(this->stations_);
    }

  if (ui_->use_dynamic_grid->isChecked() && !use_dynamic_info_ )
  {
    // turning on so clear it so only the next location update gets used
    dynamic_grid_.clear ();
  }
  use_dynamic_info_ = ui_->use_dynamic_grid->isChecked();

  write_settings ();		// make visible to all
}

void Configuration::impl::reject ()
{
  initialize_models ();		// reverts to settings as at exec ()

  // check if the Transceiver instance changed, in which case we need
  // to re open any prior Transceiver type
  if (rig_changed_)
    {
      if (have_rig_)
        {
          // we have to do this since the rig has been opened since we
          // were exec'ed even though it might fail
          open_rig ();
        }
      else
        {
          close_rig ();
        }
    }

  QDialog::reject ();
}

void Configuration::impl::on_font_push_button_clicked ()
{
  next_font_ = QFontDialog::getFont (0, next_font_, this
                                   , tr ("Font Chooser")
#if QT_VERSION >= 0x050201
                                   , QFontDialog::DontUseNativeDialog
#endif
                                   );
  ui_->font_push_button->setText(QString("Application Font (%1 %2)").arg(next_font_.family()).arg(next_font_.pointSize()));
}

void Configuration::impl::on_tableFontButton_clicked ()
{
  next_table_font_ = QFontDialog::getFont (0, next_table_font_, this
                                           , tr ("Font Chooser")
#if QT_VERSION >= 0x050201
                                           , QFontDialog::DontUseNativeDialog
#endif
                                           );
  ui_->tableFontButton->setText(QString("Table Font (%1 %2)").arg(next_table_font_.family()).arg(next_table_font_.pointSize()));
}


QColor getColor(QColor initial, QWidget *parent, QString title){
    QList<QColor> custom = {
        QColor("#66FF66"),
        QColor("#FF6666"),
        QColor("#FFEAA7"),
        QColor("#3498DB")
    };

    auto d = new QColorDialog(initial, parent);
    d->setWindowTitle(title);
    for(int i = 0; i < custom.length(); i++){
        d->setCustomColor(i, custom.at(i));
    }

    if(d->exec() == QColorDialog::Accepted){
        return d->selectedColor();
    } else {
        return initial;
    }
}

void Configuration::impl::on_pbCQmsg_clicked()
{
  auto new_color = getColor(next_color_cq_, this, "CQ Messages Color");
  if (new_color.isValid ())
    {
      next_color_cq_ = new_color;
      ui_->labCQ->setStyleSheet(QString("background: %1; color: %2").arg(next_color_cq_.name()).arg(next_color_table_foreground_.name()));
    }
}

void Configuration::impl::on_pbMyCall_clicked()
{
  auto new_color = getColor(next_color_mycall_, this, "Directed Messages Color");
  if (new_color.isValid ())
    {
      next_color_mycall_ = new_color;
      ui_->labMyCall->setStyleSheet(QString("background: %1; color: %2").arg(next_color_mycall_.name()).arg(next_color_table_foreground_.name()));
    }
}

void Configuration::impl::on_tableBackgroundButton_clicked()
{
  auto new_color = getColor(next_color_table_background_, this, "Table Background Color");
  if (new_color.isValid ())
    {
      next_color_table_background_ = new_color;
      ui_->tableBackgroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_table_background_.name()).arg(next_color_table_foreground_.name()));
    }
}

void Configuration::impl::on_tableSelectedRowBackgroundButton_clicked()
{
  auto new_color = getColor(next_color_table_highlight_, this, "Table Selected Row Background Color");
  if (new_color.isValid ())
    {
      next_color_table_highlight_ = new_color;
      ui_->tableSelectionBackgroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_table_highlight_.name()).arg(next_color_table_foreground_.name()));
    }
}

void Configuration::impl::on_tableForegroundButton_clicked()
{
  auto new_color = getColor(next_color_table_foreground_, this, "Table Foreground Color");
  if (new_color.isValid ())
    {
      next_color_table_foreground_ = new_color;
      ui_->tableBackgroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_table_background_.name()).arg(next_color_table_foreground_.name()));
      ui_->tableSelectionBackgroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_table_highlight_.name()).arg(next_color_table_foreground_.name()));
      ui_->labCQ->setStyleSheet(QString("background: %1; color: %2").arg(next_color_cq_.name()).arg(next_color_table_foreground_.name()));
      ui_->labMyCall->setStyleSheet(QString("background: %1; color: %2").arg(next_color_mycall_.name()).arg(next_color_table_foreground_.name()));
    }
}

void Configuration::impl::on_rxBackgroundButton_clicked()
{
  auto new_color = getColor(next_color_rx_background_, this, "Received Messages Background Color");
  if (new_color.isValid ())
    {
      next_color_rx_background_ = new_color;
      ui_->rxLabel->setStyleSheet(QString("background: %1; color: %2").arg(color_rx_background_.name()).arg(color_rx_foreground_.name()));
      ui_->rxForegroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_rx_background_.name()).arg(next_color_rx_foreground_.name()));
      ui_->txForegroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_rx_background_.name()).arg(next_color_tx_foreground_.name()));
    }
}

void Configuration::impl::on_rxForegroundButton_clicked()
{
  auto new_color = getColor(next_color_rx_foreground_, this, "Received Messages Foreground Color");
  if (new_color.isValid ())
    {
      next_color_rx_foreground_ = new_color;
      ui_->rxLabel->setStyleSheet(QString("background: %1; color: %2").arg(color_rx_background_.name()).arg(color_rx_foreground_.name()));
      ui_->rxForegroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_rx_background_.name()).arg(next_color_rx_foreground_.name()));
      ui_->txForegroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_rx_background_.name()).arg(next_color_tx_foreground_.name()));
    }
}

void Configuration::impl::on_rxFontButton_clicked ()
{
  next_rx_text_font_ = QFontDialog::getFont (0, next_rx_text_font_ , this
                                                  , tr ("Font Chooser")
#if QT_VERSION >= 0x050201
                                                  , QFontDialog::DontUseNativeDialog
#endif
                                                  );
  ui_->rxFontButton->setText(QString("Font (%1 %2)").arg(next_rx_text_font_.family()).arg(next_rx_text_font_.pointSize()));
}

void Configuration::impl::on_composeBackgroundButton_clicked()
{
  auto new_color = getColor(next_color_compose_background_, this, "Compose Messages Background Color");
  if (new_color.isValid ())
    {
      next_color_compose_background_ = new_color;
      ui_->composeLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_compose_background_.name()).arg(next_color_compose_foreground_.name()));
    }
}

void Configuration::impl::on_composeForegroundButton_clicked()
{
  auto new_color = getColor(next_color_compose_foreground_, this, "Compose Messages Foreground Color");
  if (new_color.isValid ())
    {
      next_color_compose_foreground_ = new_color;
      ui_->composeLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_compose_background_.name()).arg(next_color_compose_foreground_.name()));
    }
}

void Configuration::impl::on_txForegroundButton_clicked()
{
  auto new_color = getColor(next_color_tx_foreground_, this, "Transmitted Messages Foreground Color");
  if (new_color.isValid ())
    {
      next_color_tx_foreground_ = new_color;
      ui_->rxForegroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_rx_background_.name()).arg(next_color_rx_foreground_.name()));
      ui_->txForegroundLabel->setStyleSheet(QString("background: %1; color: %2").arg(next_color_rx_background_.name()).arg(next_color_tx_foreground_.name()));
    }
}

void Configuration::impl::on_txFontButton_clicked ()
{
  next_tx_text_font_ = QFontDialog::getFont (0, next_tx_text_font_ , this
                                                  , tr ("Font Chooser")
#if QT_VERSION >= 0x050201
                                                  , QFontDialog::DontUseNativeDialog
#endif
                                                  );

  ui_->txFontButton->setText(QString("Font (%1 %2)").arg(next_tx_text_font_.family()).arg(next_tx_text_font_.pointSize()));
}

void Configuration::impl::on_composeFontButton_clicked ()
{
  next_compose_text_font_ = QFontDialog::getFont (0, next_compose_text_font_ , this
                                                  , tr ("Font Chooser")
#if QT_VERSION >= 0x050201
                                                  , QFontDialog::DontUseNativeDialog
#endif
                                                  );
  ui_->composeFontButton->setText(QString("Font (%1 %2)").arg(next_compose_text_font_.family()).arg(next_compose_text_font_.pointSize()));
}

void Configuration::impl::on_PTT_port_combo_box_activated (int /* index */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_CAT_port_combo_box_activated (int /* index */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_CAT_serial_baud_combo_box_currentIndexChanged (int /* index */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_CAT_handshake_button_group_buttonClicked (int /* id */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_rig_combo_box_currentIndexChanged (int /* index */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_CAT_data_bits_button_group_buttonClicked (int /* id */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_CAT_stop_bits_button_group_buttonClicked (int /* id */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_CAT_poll_interval_spin_box_valueChanged (int /* value */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_split_mode_button_group_buttonClicked (int /* id */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_test_CAT_push_button_clicked ()
{
  if (!validate ())
    {
      return;
    }

  ui_->test_CAT_push_button->setStyleSheet ({});
  if (open_rig (true))
    {
      //Q_EMIT sync (true);
    }

  set_rig_invariants ();
}

void Configuration::impl::on_test_PTT_push_button_clicked (bool checked)
{
  ui_->test_PTT_push_button->setChecked (!checked); // let status
                                                    // update check us
  if (!validate ())
    {
      return;
    }

  if (open_rig ())
    {
      Q_EMIT self_->transceiver_ptt (checked);
    }
}

void Configuration::impl::on_force_DTR_combo_box_currentIndexChanged (int /* index */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_force_RTS_combo_box_currentIndexChanged (int /* index */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_PTT_method_button_group_buttonClicked (int /* id */)
{
  set_rig_invariants ();
}

void Configuration::impl::on_sound_input_combo_box_currentTextChanged (QString const& text)
{
  default_audio_input_device_selected_ = QAudioDeviceInfo::defaultInputDevice ().deviceName () == text;
}

void Configuration::impl::on_sound_output_combo_box_currentTextChanged (QString const& text)
{
  default_audio_output_device_selected_ = QAudioDeviceInfo::defaultOutputDevice ().deviceName () == text;
}

void Configuration::impl::on_groups_line_edit_textChanged(QString const &text)
{
}

void Configuration::impl::on_info_message_line_edit_textChanged(QString const &text)
{
}

void Configuration::impl::on_cq_message_line_edit_textChanged(QString const &text)
{
}

void Configuration::impl::on_reply_message_line_edit_textChanged(QString const &text)
{
}

void Configuration::impl::on_add_macro_line_edit_editingFinished ()
{
  ui_->add_macro_line_edit->setText (ui_->add_macro_line_edit->text ().toUpper ());
}

void Configuration::impl::on_delete_macro_push_button_clicked (bool /* checked */)
{
  auto selection_model = ui_->macros_list_view->selectionModel ();
  if (selection_model->hasSelection ())
    {
      // delete all selected items
      delete_selected_macros (selection_model->selectedRows ());
    }
}

void Configuration::impl::delete_macro ()
{
  auto selection_model = ui_->macros_list_view->selectionModel ();
  if (!selection_model->hasSelection ())
    {
      // delete item under cursor if any
      auto index = selection_model->currentIndex ();
      if (index.isValid ())
        {
          next_macros_.removeRow (index.row ());
        }
    }
  else
    {
      // delete the whole selection
      delete_selected_macros (selection_model->selectedRows ());
    }
}

void Configuration::impl::delete_selected_macros (QModelIndexList selected_rows)
{
  // sort in reverse row order so that we can delete without changing
  // indices underneath us
  qSort (selected_rows.begin (), selected_rows.end (), [] (QModelIndex const& lhs, QModelIndex const& rhs)
         {
           return rhs.row () < lhs.row (); // reverse row ordering
         });

  // now delete them
  Q_FOREACH (auto index, selected_rows)
    {
      next_macros_.removeRow (index.row ());
    }
}

void Configuration::impl::on_add_macro_push_button_clicked (bool /* checked */)
{
  if (next_macros_.insertRow (next_macros_.rowCount ()))
    {
      auto index = next_macros_.index (next_macros_.rowCount () - 1);
      ui_->macros_list_view->setCurrentIndex (index);
      next_macros_.setData (index, ui_->add_macro_line_edit->text ().toUpper());
      ui_->add_macro_line_edit->clear ();
    }
}

void Configuration::impl::delete_frequencies ()
{
  auto selection_model = ui_->frequencies_table_view->selectionModel ();
  selection_model->select (selection_model->selection (), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
  next_frequencies_.removeDisjointRows (selection_model->selectedRows ());
  ui_->frequencies_table_view->resizeColumnToContents (FrequencyList_v2::mode_column);
}

void Configuration::impl::load_frequencies ()
{
  auto file_name = QFileDialog::getOpenFileName (this, tr ("Load Working Frequencies"), writeable_data_dir_.absolutePath (), tr ("Frequency files (*.qrg);;All files (*.*)"));
  if (!file_name.isNull ())
    {
      auto const list = read_frequencies_file (file_name);
      if (list.size ()
          && (!next_frequencies_.frequency_list ().size ()
              || MessageBox::Yes == MessageBox::query_message (this
                                                               , tr ("Replace Working Frequencies")
                                                               , tr ("Are you sure you want to discard your current "
                                                                     "working frequencies and replace them with the "
                                                                     "loaded ones?"))))
        {
          next_frequencies_.frequency_list (list); // update the model
        }
    }
}

void Configuration::impl::merge_frequencies ()
{
  auto file_name = QFileDialog::getOpenFileName (this, tr ("Merge Working Frequencies"), writeable_data_dir_.absolutePath (), tr ("Frequency files (*.qrg);;All files (*.*)"));
  if (!file_name.isNull ())
    {
      next_frequencies_.frequency_list_merge (read_frequencies_file (file_name)); // update the model
    }
}

FrequencyList_v2::FrequencyItems Configuration::impl::read_frequencies_file (QString const& file_name)
{
  QFile frequencies_file {file_name};
  frequencies_file.open (QFile::ReadOnly);
  QDataStream ids {&frequencies_file};
  FrequencyList_v2::FrequencyItems list;
  quint32 magic;
  ids >> magic;
  if (qrg_magic != magic)
    {
      MessageBox::warning_message (this, tr ("Not a valid frequencies file"), tr ("Incorrect file magic"));
      return list;
    }
  quint32 version;
  ids >> version;
  // handle version checks and QDataStream version here if
  // necessary
  if (version > qrg_version)
    {
      MessageBox::warning_message (this, tr ("Not a valid frequencies file"), tr ("Version is too new"));
      return list;
    }

  // de-serialize the data using version if necessary to
  // handle old schemata
  ids >> list;

  if (ids.status () != QDataStream::Ok || !ids.atEnd ())
    {
      MessageBox::warning_message (this, tr ("Not a valid frequencies file"), tr ("Contents corrupt"));
      list.clear ();
      return list;
    }

  return list;
}

void Configuration::impl::save_frequencies ()
{
  auto file_name = QFileDialog::getSaveFileName (this, tr ("Save Working Frequencies"), writeable_data_dir_.absolutePath (), tr ("Frequency files (*.qrg);;All files (*.*)"));
  if (!file_name.isNull ())
    {
      QFile frequencies_file {file_name};
      frequencies_file.open (QFile::WriteOnly);
      QDataStream ods {&frequencies_file};
      auto selection_model = ui_->frequencies_table_view->selectionModel ();
      if (selection_model->hasSelection ()
          && MessageBox::Yes == MessageBox::query_message (this
                                                           , tr ("Only Save Selected  Working Frequencies")
                                                           , tr ("Are you sure you want to save only the "
                                                                 "working frequencies that are currently selected? "
                                                                 "Click No to save all.")))
        {
          selection_model->select (selection_model->selection (), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
          ods << qrg_magic << qrg_version << next_frequencies_.frequency_list (selection_model->selectedRows ());
        }
      else
        {
          ods << qrg_magic << qrg_version << next_frequencies_.frequency_list ();
        }
    }
}

void Configuration::impl::reset_frequencies ()
{
  if (MessageBox::Yes == MessageBox::query_message (this, tr ("Reset Working Frequencies")
                                                    , tr ("Are you sure you want to discard your current "
                                                          "working frequencies and replace them with default "
                                                          "ones?")))
    {
      next_frequencies_.reset_to_defaults ();
    }
}

void Configuration::impl::insert_frequency ()
{
  if (QDialog::Accepted == frequency_dialog_->exec ())
    {
      ui_->frequencies_table_view->setCurrentIndex (next_frequencies_.add (frequency_dialog_->item ()));
      ui_->frequencies_table_view->resizeColumnToContents (FrequencyList_v2::mode_column);
    }
}

void Configuration::impl::delete_stations ()
{
  auto selection_model = ui_->stations_table_view->selectionModel ();
  selection_model->select (selection_model->selection (), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
  next_stations_.removeDisjointRows (selection_model->selectedRows ());
  ui_->stations_table_view->resizeColumnToContents (StationList::band_column);
  ui_->stations_table_view->resizeColumnToContents (StationList::frequency_column);
  ui_->stations_table_view->resizeColumnToContents (StationList::switch_at_column);
  ui_->stations_table_view->resizeColumnToContents (StationList::switch_until_column);
}

void Configuration::impl::insert_station ()
{
  if (QDialog::Accepted == station_dialog_->exec ())
    {
      auto station = station_dialog_->station ();
      if(station.frequency_ == 0){
          Frequency l;
          Frequency h;
          if(bands_.findFreq(station.band_name_, &l, &h)){
              station.frequency_ = l;
          }
      }

      ui_->stations_table_view->setCurrentIndex (next_stations_.add (station));
      ui_->stations_table_view->resizeColumnToContents (StationList::band_column);
      ui_->stations_table_view->resizeColumnToContents (StationList::frequency_column);
      ui_->stations_table_view->resizeColumnToContents (StationList::switch_at_column);
      ui_->stations_table_view->resizeColumnToContents (StationList::switch_until_column);
    }
}

void Configuration::impl::on_save_path_select_push_button_clicked (bool /* checked */)
{
  QFileDialog fd {this, tr ("Save Directory"), ui_->save_path_display_label->text ()};
  fd.setFileMode (QFileDialog::Directory);
  fd.setOption (QFileDialog::ShowDirsOnly);
  if (fd.exec ())
    {
      if (fd.selectedFiles ().size ())
        {
          ui_->save_path_display_label->setText (fd.selectedFiles ().at (0));
        }
    }
}

void Configuration::impl::on_azel_path_select_push_button_clicked (bool /* checked */)
{
  QFileDialog fd {this, tr ("AzEl Directory"), ui_->azel_path_display_label->text ()};
  fd.setFileMode (QFileDialog::Directory);
  fd.setOption (QFileDialog::ShowDirsOnly);
  if (fd.exec ()) {
    if (fd.selectedFiles ().size ()) {
      ui_->azel_path_display_label->setText(fd.selectedFiles().at(0));
    }
  }
}

void Configuration::impl::on_sound_cq_path_select_push_button_clicked(){
    QStringList filters;
    filters << "Audio files (*.wav)"
            << "Any files (*)";

    QFileDialog fd {this, tr ("Sound File"), ui_->sound_cq_path_display_label->text ()};
    fd.setNameFilters(filters);

    if (fd.exec ()) {
      if (fd.selectedFiles ().size ()) {
        if(rig_params_.ptt_type == TransceiverFactory::PTT_method_VOX){
          QMessageBox::warning(this, "Notifications Sounds Warning", "You have enabled notification sounds while using VOX. To avoid transmitting these notification sounds, please make sure your rig is using a different sound card than your system.");
        }
        ui_->sound_cq_path_display_label->setText(fd.selectedFiles().at(0));
      }
    }
}

void Configuration::impl::on_sound_cq_path_test_push_button_clicked(){
    auto path = ui_->sound_cq_path_display_label->text();
    if(path.isEmpty()){
        return;
    }

    QSound::play(path);
}

void Configuration::impl::on_sound_cq_path_reset_push_button_clicked(){
    ui_->sound_cq_path_display_label->clear();
}

void Configuration::impl::on_sound_dm_path_select_push_button_clicked(){
    QStringList filters;
    filters << "Audio files (*.wav)"
            << "Any files (*)";

    QFileDialog fd {this, tr ("Sound File"), ui_->sound_dm_path_display_label->text ()};
    fd.setNameFilters(filters);

    if (fd.exec ()) {
      if (fd.selectedFiles ().size ()) {
        if(rig_params_.ptt_type == TransceiverFactory::PTT_method_VOX){
          QMessageBox::warning(this, "Notifications Sounds Warning", "You have enabled notification sounds while using VOX. To avoid transmitting these notification sounds, please make sure your rig is using a different sound card than your system.");
        }
        ui_->sound_dm_path_display_label->setText(fd.selectedFiles().at(0));
      }
    }
}

void Configuration::impl::on_sound_dm_path_test_push_button_clicked(){
    auto path = ui_->sound_dm_path_display_label->text();
    if(path.isEmpty()){
        return;
    }

    QSound::play(path);
}

void Configuration::impl::on_sound_dm_path_reset_push_button_clicked(){
    ui_->sound_dm_path_display_label->clear();
}

void Configuration::impl::on_sound_am_path_select_push_button_clicked(){
    QStringList filters;
    filters << "Audio files (*.wav)"
            << "Any files (*)";

    QFileDialog fd {this, tr ("Sound File"), ui_->sound_am_path_display_label->text ()};
    fd.setNameFilters(filters);

    if (fd.exec ()) {
      if (fd.selectedFiles ().size ()) {
        if(rig_params_.ptt_type == TransceiverFactory::PTT_method_VOX){
          QMessageBox::warning(this, "Notifications Sounds Warning", "You have enabled notification sounds while using VOX. To avoid transmitting these notification sounds, please make sure your rig is using a different sound card than your system.");
        }
        ui_->sound_am_path_display_label->setText(fd.selectedFiles().at(0));
      }
    }
}

void Configuration::impl::on_sound_am_path_test_push_button_clicked(){
    auto path = ui_->sound_am_path_display_label->text();
    if(path.isEmpty()){
        return;
    }

    QSound::play(path);
}

void Configuration::impl::on_sound_am_path_reset_push_button_clicked(){
    ui_->sound_am_path_display_label->clear();
}

void Configuration::impl::on_calibration_intercept_spin_box_valueChanged (double)
{
  rig_active_ = false;          // force reset
}

void Configuration::impl::on_calibration_slope_ppm_spin_box_valueChanged (double)
{
  rig_active_ = false;          // force reset
}

void Configuration::impl::on_cbFox_clicked (bool checked)
{
  if (checked) ui_->cbHound->setChecked (false);
}

void Configuration::impl::on_cbHound_clicked (bool checked)
{
  if (checked) ui_->cbFox->setChecked (false);
}

void Configuration::impl::on_cbx2ToneSpacing_clicked(bool b)
{
  if(b) ui_->cbx4ToneSpacing->setChecked(false);
}

void Configuration::impl::on_cbx4ToneSpacing_clicked(bool b)
{
  if(b) ui_->cbx2ToneSpacing->setChecked(false);
}

bool Configuration::impl::have_rig ()
{
  if (!open_rig ())
    {
      MessageBox::critical_message (this, tr ("Rig control error")
                                    , tr ("Failed to open connection to rig"));
    }
  return rig_active_;
}

bool Configuration::impl::open_rig (bool force)
{
  auto result = false;

  auto const rig_data = gather_rig_data ();
  if (force || !rig_active_ || rig_data != saved_rig_params_)
    {
      try
        {
          close_rig ();

          // create a new Transceiver object
          auto rig = transceiver_factory_.create (rig_data, transceiver_thread_);
          cached_rig_state_ = Transceiver::TransceiverState {};

          // hook up Configuration transceiver control signals to Transceiver slots
          //
          // these connections cross the thread boundary
          rig_connections_ << connect (this, &Configuration::impl::set_transceiver,
                                       rig.get (), &Transceiver::set);

          // hook up Transceiver signals to Configuration signals
          //
          // these connections cross the thread boundary
          rig_connections_ << connect (rig.get (), &Transceiver::resolution, this, [=] (int resolution) {
              rig_resolution_ = resolution;
            });
          rig_connections_ << connect (rig.get (), &Transceiver::update, this, &Configuration::impl::handle_transceiver_update);
          rig_connections_ << connect (rig.get (), &Transceiver::failure, this, &Configuration::impl::handle_transceiver_failure);

          // setup thread safe startup and close down semantics
          rig_connections_ << connect (this, &Configuration::impl::start_transceiver, rig.get (), &Transceiver::start);
          rig_connections_ << connect (this, &Configuration::impl::stop_transceiver, rig.get (), &Transceiver::stop);

          auto p = rig.release ();	// take ownership

          // schedule destruction on thread quit
          connect (transceiver_thread_, &QThread::finished, p, &QObject::deleteLater);

          // schedule eventual destruction for non-closing situations
          //
          // must   be   queued    connection   to   avoid   premature
          // self-immolation  since finished  signal  is  going to  be
          // emitted from  the object that  will get destroyed  in its
          // own  stop  slot  i.e.  a   same  thread  signal  to  slot
          // connection which by  default will be reduced  to a method
          // function call.
          connect (p, &Transceiver::finished, p, &Transceiver::deleteLater, Qt::QueuedConnection);

          ui_->test_CAT_push_button->setStyleSheet ({});
          rig_active_ = true;
          Q_EMIT start_transceiver (++transceiver_command_number_); // start rig on its thread
          result = true;
        }
      catch (std::exception const& e)
        {
          handle_transceiver_failure (e.what ());
        }

      saved_rig_params_ = rig_data;
      rig_changed_ = true;
    }
  else
    {
      result = true;
    }
  return result;
}

void Configuration::impl::set_cached_mode ()
{
  MODE mode {Transceiver::UNK};
  // override cache mode with what we want to enforce which includes
  // UNK (unknown) where we want to leave the rig mode untouched
  switch (data_mode_)
    {
    case data_mode_USB: mode = Transceiver::USB; break;
    case data_mode_data: mode = Transceiver::DIG_U; break;
    default: break;
    }

  cached_rig_state_.mode (mode);
}

void Configuration::impl::transceiver_frequency (Frequency f)
{
  cached_rig_state_.online (true); // we want the rig online
  set_cached_mode ();

  // apply any offset & calibration
  // we store the offset here for use in feedback from the rig, we
  // cannot absolutely determine if the offset should apply but by
  // simply picking an offset when the Rx frequency is set and
  // sticking to it we get sane behaviour
  cached_rig_state_.frequency (apply_calibration (f));

  Q_EMIT set_transceiver (cached_rig_state_, ++transceiver_command_number_);
}

void Configuration::impl::transceiver_tx_frequency (Frequency f)
{
  Q_ASSERT (!f || split_mode ());
  if (split_mode ())
    {
      cached_rig_state_.online (true); // we want the rig online
      set_cached_mode ();
      cached_rig_state_.split (f);
      cached_rig_state_.tx_frequency (f);

      // lookup offset for tx and apply calibration
      if (f)
        {
          // apply and offset and calibration
          // we store the offset here for use in feedback from the
          // rig, we cannot absolutely determine if the offset should
          // apply but by simply picking an offset when the Rx
          // frequency is set and sticking to it we get sane behaviour
          cached_rig_state_.tx_frequency (apply_calibration (f));
        }

      Q_EMIT set_transceiver (cached_rig_state_, ++transceiver_command_number_);
    }
}

void Configuration::impl::transceiver_mode (MODE m)
{
  cached_rig_state_.online (true); // we want the rig online
  cached_rig_state_.mode (m);
  Q_EMIT set_transceiver (cached_rig_state_, ++transceiver_command_number_);
}

void Configuration::impl::transceiver_ptt (bool on)
{
  cached_rig_state_.online (true); // we want the rig online
  set_cached_mode ();
  cached_rig_state_.ptt (on);
  Q_EMIT set_transceiver (cached_rig_state_, ++transceiver_command_number_);
}

void Configuration::impl::sync_transceiver (bool /*force_signal*/)
{
  // pass this on as cache must be ignored
  // Q_EMIT sync (force_signal);
}

void Configuration::impl::handle_transceiver_update (TransceiverState const& state,
                                                     unsigned sequence_number)
{
#if WSJT_TRACE_CAT
  qDebug () << "Configuration::handle_transceiver_update: Transceiver State #:" << sequence_number << state;
#endif

  // only follow rig on some information, ignore other stuff
  cached_rig_state_.online (state.online ());
  cached_rig_state_.frequency (state.frequency ());
  cached_rig_state_.mode (state.mode ());
  cached_rig_state_.split (state.split ());

  if (state.online ())
    {
      ui_->test_PTT_push_button->setChecked (state.ptt ());

      if (isVisible ())
        {
          ui_->test_CAT_push_button->setStyleSheet ("QPushButton {background-color: green;}");

          auto const& rig = ui_->rig_combo_box->currentText ();
          auto ptt_method = static_cast<TransceiverFactory::PTTMethod> (ui_->PTT_method_button_group->checkedId ());
          auto CAT_PTT_enabled = transceiver_factory_.has_CAT_PTT (rig);
          ui_->test_PTT_push_button->setEnabled ((TransceiverFactory::PTT_method_CAT == ptt_method && CAT_PTT_enabled)
                                                 || TransceiverFactory::PTT_method_DTR == ptt_method
                                                 || TransceiverFactory::PTT_method_RTS == ptt_method);
        }
    }
  else
    {
      close_rig ();
    }

  // pass on to clients if current command is processed
  if (sequence_number == transceiver_command_number_)
    {
      TransceiverState reported_state {state};
      // take off calibration & offset
      reported_state.frequency (remove_calibration (reported_state.frequency ()));

      if (reported_state.tx_frequency ())
        {
          // take off calibration & offset
          reported_state.tx_frequency (remove_calibration (reported_state.tx_frequency ()));
        }

      Q_EMIT self_->transceiver_update (reported_state);
    }
}

void Configuration::impl::handle_transceiver_failure (QString const& reason)
{
#if WSJT_TRACE_CAT
  qDebug () << "Configuration::handle_transceiver_failure: reason:" << reason;
#endif

  close_rig ();
  ui_->test_PTT_push_button->setChecked (false);

  if (isVisible ())
    {
      MessageBox::critical_message (this, tr ("Rig failure"), reason);
    }
  else
    {
      // pass on if our dialog isn't active
      Q_EMIT self_->transceiver_failure (reason);
    }
}

void Configuration::impl::close_rig ()
{
  ui_->test_PTT_push_button->setEnabled (false);

  // revert to no rig configured
  if (rig_active_)
    {
      ui_->test_CAT_push_button->setStyleSheet ("QPushButton {background-color: red;}");
      Q_EMIT stop_transceiver ();
      for (auto const& connection: rig_connections_)
        {
          disconnect (connection);
        }
      rig_connections_.clear ();
      rig_active_ = false;
    }
}

// load the available audio devices into the selection combo box and
// select the default device if the current device isn't set or isn't
// available
bool Configuration::impl::load_audio_devices (QAudio::Mode mode, QComboBox * combo_box, QAudioDeviceInfo * device)
{
  using std::copy;
  using std::back_inserter;

  bool result {false};

  combo_box->clear ();

  int current_index = -1;
  int default_index = -1;

  int extra_items {0};

  auto const& default_device = (mode == QAudio::AudioInput ? QAudioDeviceInfo::defaultInputDevice () : QAudioDeviceInfo::defaultOutputDevice ());

  // deal with special default audio devices on Windows
  if ("Default Input Device" == default_device.deviceName ()
      || "Default Output Device" == default_device.deviceName ())
    {
      default_index = 0;

      QList<QVariant> channel_counts;
      auto scc = default_device.supportedChannelCounts ();
      copy (scc.cbegin (), scc.cend (), back_inserter (channel_counts));

      combo_box->addItem (default_device.deviceName (), channel_counts);
      ++extra_items;
      if (default_device == *device)
        {
          current_index = 0;
          result = true;
        }
    }

  Q_FOREACH (auto const& p, QAudioDeviceInfo::availableDevices (mode))
    {
//      qDebug () << "Audio device: input:" << (QAudio::AudioInput == mode) << "name:" << p.deviceName () << "preferred format:" << p.preferredFormat () << "endians:" << p.supportedByteOrders () << "codecs:" << p.supportedCodecs () << "channels:" << p.supportedChannelCounts () << "rates:" << p.supportedSampleRates () << "sizes:" << p.supportedSampleSizes () << "types:" << p.supportedSampleTypes ();

      // convert supported channel counts into something we can store in the item model
      QList<QVariant> channel_counts;
      auto scc = p.supportedChannelCounts ();
      copy (scc.cbegin (), scc.cend (), back_inserter (channel_counts));

      combo_box->addItem (p.deviceName (), channel_counts);
      if (p == *device)
        {
          current_index = combo_box->count () - 1;
        }
      else if (p == default_device)
        {
          default_index = combo_box->count () - 1;
        }
    }
  if (current_index < 0)	// not found - use default
    {
      *device = default_device;
      result = true;
      current_index = default_index;
    }
  combo_box->setCurrentIndex (current_index);

  return result;
}

// enable only the channels that are supported by the selected audio device
void Configuration::impl::update_audio_channels (QComboBox const * source_combo_box, int index, QComboBox * combo_box, bool allow_both)
{
  // disable all items
  for (int i (0); i < combo_box->count (); ++i)
    {
      combo_box->setItemData (i, combo_box_item_disabled, Qt::UserRole - 1);
    }

  Q_FOREACH (QVariant const& v, source_combo_box->itemData (index).toList ())
    {
      // enable valid options
      int n {v.toInt ()};
      if (2 == n)
        {
          combo_box->setItemData (AudioDevice::Left, combo_box_item_enabled, Qt::UserRole - 1);
          combo_box->setItemData (AudioDevice::Right, combo_box_item_enabled, Qt::UserRole - 1);
          if (allow_both)
            {
              combo_box->setItemData (AudioDevice::Both, combo_box_item_enabled, Qt::UserRole - 1);
            }
        }
      else if (1 == n)
        {
          combo_box->setItemData (AudioDevice::Mono, combo_box_item_enabled, Qt::UserRole - 1);
        }
    }
}

// load all the supported rig names into the selection combo box
void Configuration::impl::enumerate_rigs ()
{
  ui_->rig_combo_box->clear ();

  auto rigs = transceiver_factory_.supported_transceivers ();

  for (auto r = rigs.cbegin (); r != rigs.cend (); ++r)
    {
      if ("None" == r.key ())
        {
          // put None first
          ui_->rig_combo_box->insertItem (0, r.key (), r.value ().model_number_);
        }
      else
        {
          int i;
          for(i=1;i<ui_->rig_combo_box->count() && (r.key().toLower() > ui_->rig_combo_box->itemText(i).toLower());++i);
          if (i < ui_->rig_combo_box->count())  ui_->rig_combo_box->insertItem (i, r.key (), r.value ().model_number_);
          else ui_->rig_combo_box->addItem (r.key (), r.value ().model_number_);
        }
    }

  ui_->rig_combo_box->setCurrentText (rig_params_.rig_name);
}

void Configuration::impl::fill_port_combo_box (QComboBox * cb)
{
  auto current_text = cb->currentText ();
  cb->clear ();
  Q_FOREACH (auto const& p, QSerialPortInfo::availablePorts ())
    {
      if (!p.portName ().contains ( "NULL" )) // virtual serial port pairs
        {
          // remove possibly confusing Windows device path (OK because
          // it gets added back by Hamlib)
          cb->addItem (p.systemLocation ().remove (QRegularExpression {R"(^\\\\\.\\)"}));
        }
    }
  cb->addItem("USB");
  cb->setEditText (current_text);
}

auto Configuration::impl::apply_calibration (Frequency f) const -> Frequency
{
  if (frequency_calibration_disabled_) return f;
  return std::llround (calibration_.intercept
                       + (1. + calibration_.slope_ppm / 1.e6) * f);
}

auto Configuration::impl::remove_calibration (Frequency f) const -> Frequency
{
  if (frequency_calibration_disabled_) return f;
  return std::llround ((f - calibration_.intercept)
                       / (1. + calibration_.slope_ppm / 1.e6));
}

#if !defined (QT_NO_DEBUG_STREAM)
ENUM_QDEBUG_OPS_IMPL (Configuration, DataMode);
ENUM_QDEBUG_OPS_IMPL (Configuration, Type2MsgGen);
#endif

ENUM_QDATASTREAM_OPS_IMPL (Configuration, DataMode);
ENUM_QDATASTREAM_OPS_IMPL (Configuration, Type2MsgGen);

ENUM_CONVERSION_OPS_IMPL (Configuration, DataMode);
ENUM_CONVERSION_OPS_IMPL (Configuration, Type2MsgGen);
