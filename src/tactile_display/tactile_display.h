// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
#ifndef EXAMPLES_TACTILE_DISPLAY_TACTILE_DISPLAY_H_
#define EXAMPLES_TACTILE_DISPLAY_TACTILE_DISPLAY_H_

#include <memory>
#include <QWidget>

namespace ghand { class GHand; }

// Caller owns the connected hand and must keep it alive until this widget is
// destroyed. The widget owns tactile acquisition only; it sends no motion.
// It exclusively uses the hand's tactile callback slot while active.
class TactileDisplay : public QWidget {
 public:
  explicit TactileDisplay(ghand::GHand* hand, QWidget* parent = nullptr);
  ~TactileDisplay() override;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif
