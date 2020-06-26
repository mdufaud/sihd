#!/usr/bin/python
# coding: utf-8

class AObservable(object):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.__observers = set()

    def add_observer(self, observer):
        if not getattr(observer, "on_notify"):
            raise NotImplementedError(str(observer) +
                    " does not implement 'on_notify'")
        self.__observers.add(observer)

    def notify_observers(self):
        for observer in self.__observers:
            observer.on_notify(self)

    def remove_observer(self, observer):
        try:
            self.__observers.remove(observer)
        except KeyError:
            return False
        return True

    def clear_observers(self):
        self.__observers = set()
