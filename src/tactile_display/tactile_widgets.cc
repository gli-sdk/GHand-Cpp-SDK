// SPDX-FileCopyrightText: 2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only
// Layout and force markers follow GHand-Python-SDK tactile_display/widgets.py.
#include "tactile_display/tactile_widgets.h"
#include "tactile_display/tactile_points.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace {
constexpr double kPi = 3.14159265358979323846;
const char* const kNames[] = {"Thumb", "Index Finger", "Middle Finger", "Ring Finger", "Little Finger"};
bool Valid(const Force3& force) {
  return std::all_of(force.begin(), force.end(), [](float value) { return std::isfinite(value); });
}
struct ColorStop { double value; const char* color; };
const ColorStop kDotStops[] = {{.1,"#0DFF00"},{6.25,"#EEFF00"},{12.5,"#FFB700"},{18.75,"#FF8502"},{25,"#FF0004"}};
const ColorStop kArrowStops[] = {{0,"#0DFF00"},{2.5,"#EEFF00"},{5,"#FFB700"},{7.5,"#FF8502"},{10,"#FF0004"}};
QColor ForceColor(double value, const ColorStop* stops, QColor fallback) {
  if (value < stops[0].value) return fallback;
  if (value >= stops[4].value) return QColor(stops[4].color);
  for (int i = 0; i < 4; ++i) {
    if (value < stops[i].value || value > stops[i+1].value) continue;
    const double ratio = (value-stops[i].value)/(stops[i+1].value-stops[i].value);
    const QColor start(stops[i].color), end(stops[i+1].color);
    return QColor(int(start.red()+ratio*(end.red()-start.red())),
                  int(start.green()+ratio*(end.green()-start.green())),
                  int(start.blue()+ratio*(end.blue()-start.blue())));
  }
  return QColor(stops[4].color);
}
int Round(double value) { return int(std::floor(value+.5)); }

void DrawMarker(QPainter& p, QPointF center, const Force3& force,
                double scale, bool overview, double rotation = 0) {
  const double magnitude = std::min(std::hypot(force[0], force[1]), 15.0f);
  const int rounded_magnitude = Round(magnitude);
  if (rounded_magnitude > 0) {
    const int arrow = overview ? std::min(8+(rounded_magnitude-1)*2,44) : std::min(3+rounded_magnitude-1,12);
    const double radius = (overview ? 49.0 : 27.0)/2;
    const double angle = std::atan2(-force[1],force[0])*180/kPi+90+rotation;
    p.save(); p.translate(center); p.rotate(angle);
    const QColor color = ForceColor(magnitude,kArrowStops,QColor("#0DFF00"));
    QPainterPath triangle, circle;
    QPolygonF polygon;
    polygon << QPointF(0,-(radius+arrow)*scale)
            << QPointF(radius*std::cos(240*kPi/180)*scale,radius*std::sin(240*kPi/180)*scale)
            << QPointF(radius*std::cos(300*kPi/180)*scale,radius*std::sin(300*kPi/180)*scale);
    triangle.addPolygon(polygon);
    circle.addEllipse(QPointF(),radius*scale,radius*scale);
    p.setBrush(color); p.setPen(QPen(color,std::max(scale,1.0)));
    p.drawPath(triangle.subtracted(circle));
    if (overview) { p.setBrush(Qt::NoBrush); p.drawEllipse(QPointF(),radius*scale,radius*scale); }
    p.restore();
  }
  const double value = std::clamp(double(force[2]),0.0,25.0);
  const int rounded = Round(value);
  const int diameter = overview ? (rounded <= 0 ? 0 : std::min(7+(rounded-1)*2,41)) :
                                  (rounded <= 0 ? 2 : std::min(4+rounded-1,25));
  if (diameter <= 0) return;
  const QColor color = ForceColor(value,kDotStops,overview ? QColor(0,255,0) : QColor(255,255,255,128));
  const double radius = diameter*scale/2, glow = radius*2;
  QRadialGradient gradient(center,glow);
  gradient.setColorAt(.5,QColor(color.red(),color.green(),color.blue(),90));
  gradient.setColorAt(.7,QColor(color.red(),color.green(),color.blue(),70));
  gradient.setColorAt(1,QColor(color.red(),color.green(),color.blue(),0));
  p.setPen(Qt::NoPen); p.setBrush(gradient); p.drawEllipse(center,glow,glow);
  p.setBrush(color); p.drawEllipse(center,radius,radius);
}

// Qt Widgets equivalent of the five pyqtgraph panels, with the same axis
// ranges, colors, titles, legends and timestamp-based 60-second history.
class ForcePlot : public QWidget {
 public:
  ForcePlot(int finger, const ForceHistory* history, const double* now, QWidget* parent)
      : QWidget(parent), finger_(finger), history_(history), now_(now) { setMinimumHeight(100); }
 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(),Qt::black);
    p.setPen(QColor("#969696"));
    QFont title = p.font(); title.setPixelSize(14); p.setFont(title);
    p.drawText(QRectF(80,0,width()-80,30),Qt::AlignCenter,kNames[finger_]);
    QFont font = p.font(); font.setPixelSize(12); p.setFont(font);
    const QRectF area(80,30,std::max(1,width()-90),std::max(1,height()-50));
    const auto y = [&area](double value) { return area.bottom()-(value+10)/35*area.height(); };
    // Minor/major grid lines match pyqtgraph's dark background.
    for (int time = -60; time <= 0; ++time) {
      p.setPen(QColor(time%5 == 0 ? "#292929" : "#111111"));
      const double x = area.right()+time/60.0*area.width(); p.drawLine(QPointF(x,area.top()),QPointF(x,area.bottom()));
      if (time%5 == 0 && time > -60 && time < 0) {
        p.setPen(QColor("#969696"));
        p.drawText(QRectF(x-18,area.bottom()+2,36,18),Qt::AlignCenter,QString::number(time));
      }
    }
    for (int value = -10; value <= 25; ++value) {
      p.setPen(QColor(value%10 == 0 ? "#292929" : "#111111"));
      p.drawLine(QPointF(area.left(),y(value)),QPointF(area.right(),y(value)));
      if (value%10 == 0 && value > -10) {
        p.setPen(QColor("#969696"));
        p.drawText(QRectF(44,y(value)-9,30,18),Qt::AlignRight|Qt::AlignVCenter,QString::number(value));
      }
    }
    p.setPen(QColor("#969696"));
    p.drawLine(area.topLeft(),area.bottomLeft()); p.drawLine(area.bottomLeft(),area.bottomRight());
    p.drawText(QRectF(0,area.center().y()-12,40,24),Qt::AlignCenter,"F (N)");
    const QColor colors[] = {QColor("#e74c3c"),QColor("#27ae60"),QColor("#3498db")};
    const char* axes[] = {"Fx","Fy","Fz"};
    p.save(); p.setClipRect(area);
    for (int axis = 0; axis < 3; ++axis) {
      QPainterPath path; bool started = false;
      for (const auto& sample : *history_) {
        const double age = sample.time-*now_;
        const float value = sample.forces[finger_][axis];
        if (!std::isfinite(value)) { started = false; continue; }
        const QPointF point(area.right()+age/60*area.width(),y(value));
        if (started) path.lineTo(point); else path.moveTo(point);
        started = true;
      }
      p.setPen(QPen(colors[axis],1.5)); p.drawPath(path);
    }
    p.restore();
    for (int axis = 0; axis < 3; ++axis) {
      const double top = area.top()+25+axis*21;
      p.setPen(QPen(colors[axis],1.5)); p.drawLine(QPointF(area.left()+14,top),QPointF(area.left()+35,top));
      p.setPen(QColor("#969696")); p.drawText(QRectF(area.left()+43,top-9,30,18),Qt::AlignVCenter,axes[axis]);
    }
  }
 private:
  int finger_;
  const ForceHistory* history_;
  const double* now_;
};
}  // namespace

TactileSwitch::TactileSwitch(QWidget* parent) : QAbstractButton(parent) {
  setText("Tactile"); setCheckable(true); setCursor(Qt::PointingHandCursor);
}
QSize TactileSwitch::sizeHint() const { return QSize(fontMetrics().horizontalAdvance(text())+8+44,30); }
void TactileSwitch::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  if (!isEnabled()) p.setOpacity(.4);
  const int label = fontMetrics().horizontalAdvance(text());
  const QRectF track(label+8,(height()-22)/2.0,44,22);
  p.setPen(Qt::NoPen); p.setBrush(QColor(isChecked() ? "#2e7d32" : "#b8b8b8"));
  p.drawRoundedRect(track,11,11); p.setBrush(Qt::white);
  p.drawEllipse(QRectF(isChecked() ? track.right()-20 : track.left()+2,track.top()+2,18,18));
  p.setPen(palette().color(QPalette::WindowText));
  p.drawText(QRectF(0,0,label+2,height()),Qt::AlignVCenter|Qt::AlignLeft,text());
}

FingerView::FingerView(int finger,QWidget* parent) : QWidget(parent),finger_(finger) {
  const QString stem = finger == 0 ? "thumb" : "four_fingers";
  background_.load(":/ghand/tactile/"+stem+".png"); overlay_.load(":/ghand/tactile/"+stem+"1.png");
  setMinimumSize(150,220); setMouseTracking(true);
  setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
}
int FingerView::PointCount() const { return finger_ == 0 ? 38 : 31; }
void FingerView::SetData(const ghand::TactileData& data) {
  forces_.clear();
  if (data.state && data.distributed_force.size() == size_t(PointCount()*3) &&
      std::all_of(data.distributed_force.begin(),data.distributed_force.end(),[](float v) { return std::isfinite(v); })) {
    for (size_t i = 0; i < data.distributed_force.size(); i += 3)
      forces_.push_back({{data.distributed_force[i],data.distributed_force[i+1],data.distributed_force[i+2]}});
  }
  update();
}
void FingerView::Clear() { forces_.clear(); update(); }
QRectF FingerView::ImageRect(double* scale) const {
  const QRectF available = QRectF(rect()).adjusted(6,28,-6,-6);
  *scale = std::min(available.width()/312,available.height()/419);
  const QSizeF size(312 * *scale,419 * *scale);
  return QRectF(available.center()-QPointF(size.width()/2,size.height()/2),size);
}
QPointF FingerView::MappedPoint(int index) const {
  double scale; const QRectF image = ImageRect(&scale);
  const QPointF point = (finger_ == 0 ? kThumb : kFinger)[index];
  return image.topLeft()+QPointF(11+point.x(),21+point.y())*scale;
}
void FingerView::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  auto font = p.font(); font.setPointSize(11); font.setBold(true); p.setFont(font);
  p.setPen(QColor("#dbe8f5")); p.drawText(QRectF(0,3,width(),24),Qt::AlignCenter,kNames[finger_]);
  double scale; const QRectF image = ImageRect(&scale);
  p.drawPixmap(image,background_,QRectF(background_.rect()));
  p.drawPixmap(QRectF(image.left()+8*scale,image.top()+18*scale,292*scale,370*scale),overlay_,QRectF(overlay_.rect()));
  for (int i = 0; i < PointCount(); ++i)
    DrawMarker(p,MappedPoint(i),forces_.empty() ? Force3{{0,0,0}} : forces_[i],scale,false);
}
void FingerView::mouseMoveEvent(QMouseEvent* event) {
  if (forces_.empty()) { setToolTip("No valid data available"); return; }
  int nearest = 0; double distance = std::numeric_limits<double>::infinity();
  for (int i = 0; i < PointCount(); ++i) {
    const double next = (MappedPoint(i)-event->position()).manhattanLength();
    if (next < distance) { distance = next; nearest = i; }
  }
  const auto& force = forces_[nearest];
  setToolTip(distance < 16 ? QString("#%1: Fx %2 N, Fy %3 N, Fz %4 N")
      .arg(nearest).arg(force[0],0,'f',1).arg(force[1],0,'f',1).arg(force[2],0,'f',1) : QString());
}

HandOverview::HandOverview(bool left,QWidget* parent) : QWidget(parent),left_(left),
    hand_(left ? ":/ghand/tactile/left_hand_icon.png" : ":/ghand/tactile/right_hand_icon.png") {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  forces_.fill({{nan,nan,nan}}); setMinimumHeight(300);
}
void HandOverview::SetForces(const FingerForces& forces) { forces_ = forces; update(); }
void HandOverview::paintEvent(QPaintEvent*) {
  QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
  const double scale = std::min(width()/586.0,height()/625.0);
  const QRectF scene((width()-586*scale)/2,(height()-625*scale)/2,586*scale,625*scale);
  if (hand_.isNull()) return;
  const double image_scale = std::min(scene.width()/hand_.width(),scene.height()/hand_.height());
  const QSizeF size(hand_.width()*image_scale,hand_.height()*image_scale);
  p.drawPixmap(QRectF(scene.center()-QPointF(size.width()/2,size.height()/2),size),hand_,QRectF(hand_.rect()));
  const QPointF right[] = {{443,179},{252,-14},{168,-18},{91,-8},{20,53}};
  const QPointF left[] = {{42,235},{220,-3},{303,-21},{381,-17},{452,33}};
  const QPointF right_bars[] = {{404,213},{323,30},{183,18},{109,35},{43,113}};
  const QPointF left_bars[] = {{155,211},{240,53},{379,19},{451,35},{520,106}};
  const double right_angles[] = {30,6.77,0,-4,-8.04};
  for (int finger = 0; finger < 5; ++finger) {
    if (!Valid(forces_[finger])) continue;
    const double rotation = right_angles[finger]*(left_ ? -1 : 1), angle = rotation*kPi/180;
    const QPointF offset(58*std::cos(angle)-58*std::sin(angle),58*std::sin(angle)+58*std::cos(angle));
    DrawMarker(p,scene.topLeft()+((left_ ? left : right)[finger]+offset)*scale,forces_[finger],scale,true,rotation);
    const QPointF bar_pos = scene.topLeft()+(left_ ? left_bars : right_bars)[finger]*scale;
    const float fz = forces_[finger][2];
    p.save(); auto font = p.font(); font.setPixelSize(std::max(8,Round(13*scale))); p.setFont(font);
    p.setPen(Qt::white);
    const double text_x = bar_pos.x()-8*scale;
    p.drawText(QRectF(text_x,bar_pos.y()+5*scale,40*scale,18*scale),Qt::AlignCenter,QString::number(fz,'f',1));
    const QRectF bar(bar_pos.x()+8*scale,bar_pos.y()+29*scale,8*scale,31*scale);
    p.setPen(Qt::NoPen); p.setBrush(QColor("#D5D5D5")); p.drawRoundedRect(bar,4*scale,4*scale);
    const double ratio = std::clamp(double(fz),0.0,25.0)/25;
    if (ratio > 0) {
      p.setBrush(ForceColor(fz,kDotStops,QColor("#0DFF00")));
      p.drawRoundedRect(QRectF(bar.left(),bar.bottom()-bar.height()*ratio,bar.width(),bar.height()*ratio),4*scale,4*scale);
    }
    p.setPen(Qt::white); p.drawText(QRectF(text_x,bar_pos.y()+66*scale,40*scale,18*scale),Qt::AlignCenter,"Fz");
    p.restore();
  }
}

ForcePlots::ForcePlots(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(3);
  for (int i = 0; i < 5; ++i) {
    plots_[i] = new ForcePlot(i,&history_,&now_,this); layout->addWidget(plots_[i],1);
  }
}
void ForcePlots::SetHistory(const ForceHistory& history,double now) {
  history_ = history; now_ = now;
  for (auto* plot : plots_) plot->update();
}
