#include "afcscrollarea.h"

void AfcScrollArea::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    emit mouseDown();
}

void AfcScrollArea::wheelEvent(QWheelEvent *event)
{
    if((event->modifiers() & Qt::ControlModifier) != 0) {
        event->ignore();
        emit zoomStepped(event->delta() / 120);
    }
    else event->accept();
}
