#ifndef AFCSCROLLAREA_H
#define AFCSCROLLAREA_H

#include <QScrollArea>
#include <QMouseEvent>
#include <QWheelEvent>

class AfcScrollArea : public QScrollArea
{
  Q_OBJECT
  protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
  signals:
    void mouseDown();
    void zoomStepped(int);
  public:
     explicit AfcScrollArea(QWidget* parent=0) : QScrollArea(parent) { }
    ~AfcScrollArea() { }

};

#endif // AFCSCROLLAREA_H
