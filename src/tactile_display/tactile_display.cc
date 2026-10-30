// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
#include "tactile_display/tactile_display.h"
#include "tactile_display/tactile_widgets.h"
#include "tactile_display/tactile_acquisition.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include "ghand/ghand.h"

static void InitializeTactileResources() { Q_INIT_RESOURCE(tactile_assets); }

struct TactileDisplay::Impl {
  TactileDisplay* window;
  ghand::GHand* hand;
  QThread thread;
  QObject* worker = new QObject;
  QTimer* refresh;
  TactileSwitch* toggle;
  QPushButton* zero;
  QLabel* status;
  std::array<FingerView*, 5> fingers;
  HandOverview* overview;
  ForcePlots* plots;
  std::shared_ptr<Acquisition> acquisition = std::make_shared<Acquisition>();
  bool opened = false, busy = false, closing = false, allow_close = false;
  bool worker_opened = false;  // Accessed only on the SDK worker thread.
  QString command_error;

  Impl(TactileDisplay* owner, ghand::GHand* device) : window(owner), hand(device) {
    InitializeTactileResources();
    QPalette palette = window->palette();
    palette.setColor(QPalette::Window,Qt::black);
    palette.setColor(QPalette::WindowText,QColor("#e0e0e0"));
    window->setPalette(palette); window->setAutoFillBackground(true);
    auto* root = new QVBoxLayout(window); root->setContentsMargins(0,0,0,0);
    auto* split = new QSplitter(Qt::Horizontal,window);
    auto* left = new QWidget;
    auto* layout = new QVBoxLayout(left);
    auto* control_widget = new QWidget;
    auto* controls = new QHBoxLayout;
    control_widget->setLayout(controls);
    toggle = new TactileSwitch; zero = new QPushButton("Zero Tactile");
    status = new QLabel(hand ? "Connected; tactile closed" : "Preview; no device");
    controls->addWidget(toggle); controls->addWidget(zero); controls->addWidget(status, 1);
    layout->addWidget(control_widget);
    auto* hand_group = new QWidget;
    auto* hand_layout = new QVBoxLayout(hand_group);
    overview = new HandOverview(hand && hand->GetHandType() == ghand::HandType::LEFT_HAND);
    hand_layout->addWidget(overview); layout->addWidget(hand_group,5);
    auto* finger_group = new QWidget;
    auto* row = new QHBoxLayout;
    finger_group->setLayout(row);
    for (int i = 0; i < 5; ++i) { fingers[i] = new FingerView(i); row->addWidget(fingers[i]); }
    layout->addWidget(finger_group,2);
    auto* plot_group = new QWidget;
    auto* plot_layout = new QVBoxLayout(plot_group);
    plots = new ForcePlots; plot_layout->addWidget(plots);
    split->addWidget(left); split->addWidget(plot_group);
    split->setStretchFactor(0,9); split->setStretchFactor(1,7);
    split->setSizes({900,700}); root->addWidget(split);
    worker->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    thread.start();
    QObject::connect(toggle, &QAbstractButton::toggled, window, [this](bool checked) { command(checked ? 0 : 1); });
    QObject::connect(zero, &QPushButton::clicked, window, [this] { command(2); });
    refresh = new QTimer(window);
    refresh->setInterval(33);
    QObject::connect(refresh, &QTimer::timeout, window, [this] { redraw(); });
    refresh->start(); buttons();
  }
  void buttons() {
    toggle->setEnabled(hand && !busy && !closing);
    zero->setEnabled(hand && opened && !busy && !closing);
    const QSignalBlocker blocker(toggle);
    toggle->setChecked(opened);
  }
  void clear(bool history) {
    acquisition->Clear(history);
    for (auto* finger : fingers) finger->Clear();
    overview->SetForces(InvalidForces());
  }
  void subscribe() {
    auto state = acquisition;
    unsigned generation;
    { std::lock_guard<std::mutex> lock(state->mutex); generation = state->generation; }
    hand->SetTactileDataCallback([state, generation](const Frame& frame) {
      state->Ingest(frame,generation,TactileNow());
    });
  }
  void command(int operation) {
    command_error.clear();
    busy = true; buttons();
    if (operation != 2) clear(operation == 0);
    status->setText(operation == 2 ? "Zeroing; keep sensors unloaded..." : "Updating tactile...");
    QMetaObject::invokeMethod(worker, [this, operation] {
      // Only the worker issues SDK commands; callbacks never access the UI.
      hand->SetTactileDataCallback(nullptr);
      const auto result = operation == 0 ? hand->OpenTactile() :
                          operation == 1 ? hand->CloseTactile() : hand->ZeroTactile();
      if (operation == 2 && result) acquisition->Clear(true);
      if (operation == 0 && result) worker_opened = true;
      if (operation == 1 && result) worker_opened = false;
      if (worker_opened) subscribe();
      QMetaObject::invokeMethod(window, [this, operation, result] {
        if (operation == 0) opened = static_cast<bool>(result);
        if (operation == 1 && result) opened = false;
        busy = false; buttons();
        if (!result) command_error = QString::fromStdString(result.message);
        status->setText(result ? (opened ? "Tactile open; waiting for data" : "Tactile closed") :
                        QString::fromStdString(result.message));
      }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
  }
  void redraw() {
    Frame frame;
    ForceHistory history;
    const double now = TactileNow();
    acquisition->Drain(now,&frame,&history);
    plots->SetHistory(history,now);
    overview->SetForces(history.empty() ? InvalidForces() : history.back().forces);
    for (int i = 0; i < 5; ++i) {
      const auto found = frame.find(static_cast<ghand::TactileSensorId>(i));
      fingers[i]->SetData(found == frame.end() ? ghand::TactileData{} : found->second);
    }
  }
};

TactileDisplay::TactileDisplay(ghand::GHand* hand, QWidget* parent)
    : QWidget(parent), impl_(new Impl(this, hand)) {
  setWindowTitle("GHand Tactile Display"); resize(1600, 1000);
}

TactileDisplay::~TactileDisplay() {
  impl_->refresh->stop();
  // Also covers destruction without a close event. Worker queues are drained
  // before joining, so neither commands nor callbacks outlive the display.
  if (impl_->thread.isRunning()) {
    QMetaObject::invokeMethod(impl_->worker, [this] {
      if (impl_->hand) {
        impl_->hand->SetTactileDataCallback(nullptr);
        if (impl_->worker_opened) impl_->hand->CloseTactile();
      }
    }, Qt::BlockingQueuedConnection);
    impl_->thread.quit(); impl_->thread.wait();
  }
}

void TactileDisplay::closeEvent(QCloseEvent* event) {
  if (impl_->allow_close) { event->accept(); return; }
  event->ignore();
  if (impl_->closing) return;
  impl_->closing = true; impl_->refresh->stop(); impl_->buttons(); impl_->clear(false);
  impl_->status->setText("Closing tactile...");
  QMetaObject::invokeMethod(impl_->worker, [this] {
    ghand::Result result;
    if (impl_->hand) {
      impl_->hand->SetTactileDataCallback(nullptr);
      if (impl_->worker_opened) {
        result = impl_->hand->CloseTactile();
        if (result) impl_->worker_opened = false;
        else impl_->subscribe();
      }
    }
    QMetaObject::invokeMethod(this, [this, result] {
      if (!result) {
        impl_->closing = false; impl_->buttons();
        impl_->command_error = "Close failed: " + QString::fromStdString(result.message);
        impl_->status->setText(impl_->command_error);
        impl_->refresh->start();
        return;
      }
      impl_->allow_close = true; close();
    }, Qt::QueuedConnection);
  }, Qt::QueuedConnection);
}
