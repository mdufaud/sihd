#!/usr/bin/python
#coding: utf-8

""" System """

from sihd.srcs import Core

class IInteractor(Core.IThreadedService, Core.ILoggable,
                    Core.IAppContainer):

    def __init__(self, app=None, name="IInteractor"):
        super(IInteractor, self).__init__(name)
        if (app):
            self.set_app(app)
        self.set_run_method(self.interact)
        self._interaction = None

    def set_app(self, app):
        super(IInteractor, self).set_app(app)
        app.add_interactor(self)

    def set_interaction(self, action):
        self._interaction = action

    def interact(self, action=None, *args, **kwargs):
        if action is None:
            action = self._interaction
        return self._interact_impl(action, *args, **kwargs)

    def _interact_impl(self, action):
        raise NotImplementedError("Interact not implemented");
