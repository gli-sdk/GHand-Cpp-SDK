// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
#ifndef SRC_TACTILE_DISPLAY_TACTILE_WIDGETS_H_
#define SRC_TACTILE_DISPLAY_TACTILE_WIDGETS_H_

#include <array>
#include <deque>
#include <QAbstractButton>
#include <QPixmap>
#include <QWidget>
#include "ghand/types.h"

using Force3 = std::array<float, 3>;
using FingerForces = std::array<Force3, 5>;
struct ForceSample { double time; FingerForces forces; };
using ForceHistory = std::deque<ForceSample>;

class TactileSwitch : public QAbstractButton {
 public:
  explicit TactileSwitch(QWidget* parent = nullptr);
  QSize sizeHint() const override;
 protected:
  void paintEvent(QPaintEvent*) override;
};

class FingerView : public QWidget {
 public:
  explicit FingerView(int finger, QWidget* parent = nullptr);
  void SetData(const ghand::TactileData& data);
  void Clear();
  int PointCount() const;
 protected:
  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
 private:
  QRectF ImageRect(double* scale) const;
  QPointF MappedPoint(int index) const;
  int finger_;
  QPixmap background_, overlay_;
  std::vector<Force3> forces_;
};

class HandOverview : public QWidget {
 public:
  explicit HandOverview(bool left, QWidget* parent = nullptr);
  void SetForces(const FingerForces& forces);
 protected:
  void paintEvent(QPaintEvent*) override;
 private:
  bool left_;
  QPixmap hand_;
  FingerForces forces_;
};

class ForcePlots : public QWidget {
 public:
  explicit ForcePlots(QWidget* parent = nullptr);
  void SetHistory(const ForceHistory& history, double now);
 private:
  std::array<QWidget*, 5> plots_;
  ForceHistory history_;
  double now_ = 0;
};

#endif
