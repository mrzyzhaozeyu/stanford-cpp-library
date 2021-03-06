/*
 * File: geventqueue.cpp
 * ---------------------
 *
 * @author Marty Stepp
 * @version 2018/08/23
 * - renamed to geventqueue.cpp
 * @version 2018/07/03
 * - initial version
 */

#define INTERNAL_INCLUDE 1
#include "qtgui.h"
#include <QEvent>
#include <QThread>
#define INTERNAL_INCLUDE 1
#include "error.h"
#define INTERNAL_INCLUDE 1
#include "exceptions.h"
#define INTERNAL_INCLUDE 1
#include "gwindow.h"
#define INTERNAL_INCLUDE 1
#include "strlib.h"
#undef INTERNAL_INCLUDE

#ifdef _WIN32
#  include <direct.h>   // for chdir
#else // _WIN32
#  include <unistd.h>   // for chdir
#endif // _WIN32

GEventQueue* GEventQueue::_instance = nullptr;

GEventQueue::GEventQueue()
        : _eventMask(0) {
    // empty
}

GThunk GEventQueue::dequeue() {
    _functionQueueMutex.lockForWrite();
    GThunk thunk = _functionQueue.dequeue();
    _functionQueueMutex.unlock();
    return thunk;
}

void GEventQueue::enqueueEvent(const GEvent& event) {
    if (isAcceptingEvent(event.getEventClass())) {
        _eventQueueMutex.lockForWrite();
        _eventQueue.enqueue(event);
        _eventQueueMutex.unlock();
    }
}

int GEventQueue::getEventMask() const {
    return _eventMask;
}

GEvent GEventQueue::getNextEvent(int mask) {
    setEventMask(mask);

    // check if any events have arrived
    _eventQueueMutex.lockForRead();
    bool empty = _eventQueue.isEmpty();
    _eventQueueMutex.unlock();

    if (!empty) {
        // grab the event and return it
        _eventQueueMutex.lockForWrite();
        while (!_eventQueue.isEmpty()) {
            GEvent event = _eventQueue.dequeue();
            if (isAcceptingEvent(event)) {
                _eventQueueMutex.unlock();
                return event;
            }
        }
        _eventQueueMutex.unlock();
    }

    GEvent bogusEvent;
    return bogusEvent;
}

GEventQueue* GEventQueue::instance() {
    if (!_instance) {
        _instance = new GEventQueue();
    }
    return _instance;
}

bool GEventQueue::isAcceptingEvent(const GEvent& event) const {
    return isAcceptingEvent(event.getEventClass());
}

bool GEventQueue::isAcceptingEvent(int eventClass) const {
    return (_eventMask & eventClass) != 0;
}

bool GEventQueue::isEmpty() const {
    return _functionQueue.isEmpty();
}

GThunk GEventQueue::peek() {
    _functionQueueMutex.lockForRead();
    GThunk thunk = _functionQueue.peek();
    _functionQueueMutex.unlock();
    return thunk;
}

void GEventQueue::runOnQtGuiThreadAsync(GThunk thunk) {
    _functionQueueMutex.lockForWrite();
    _functionQueue.add(thunk);
    _functionQueueMutex.unlock();
    emit eventReady();
}

void GEventQueue::runOnQtGuiThreadSync(GThunk thunk) {
    _functionQueueMutex.lockForWrite();
    _functionQueue.add(thunk);
    _functionQueueMutex.unlock();
    emit eventReady();

    // TODO: "empty" is not quite right condition
    while (true) {
        _functionQueueMutex.lockForRead();
        bool empty = _functionQueue.isEmpty();
        _functionQueueMutex.unlock();
        if (empty) {
            break;
        } else {
            GThread::sleep(1);
        }
    }
}

void GEventQueue::setEventMask(int mask) {
    _eventMask = mask;
}

GEvent GEventQueue::waitForEvent(int mask) {
    setEventMask(mask);
    while (true) {
        // check if any events have arrived
        _eventQueueMutex.lockForRead();
        bool empty = _eventQueue.isEmpty();
        _eventQueueMutex.unlock();

        if (!empty) {
            // grab the event and return it
            _eventQueueMutex.lockForWrite();
            while (!_eventQueue.isEmpty()) {
                GEvent event = _eventQueue.dequeue();
                if (isAcceptingEvent(event)) {
                    _eventQueueMutex.unlock();
                    return event;
                }
            }
            _eventQueueMutex.unlock();
        }

        GThread::sleep(1);
    }
}

GEvent getNextEvent(int mask) {
    return GEventQueue::instance()->getNextEvent(mask);
}

GMouseEvent waitForClick() {
    return GEventQueue::instance()->waitForEvent(CLICK_EVENT);
}

GEvent waitForEvent(int mask) {
    return GEventQueue::instance()->waitForEvent(mask);
}

#ifdef SPL_PRECOMPILE_QT_MOC_FILES
#define INTERNAL_INCLUDE 1
#include "moc_geventqueue.cpp"   // speeds up compilation of auto-generated Qt files
#undef INTERNAL_INCLUDE
#endif // SPL_PRECOMPILE_QT_MOC_FILES
