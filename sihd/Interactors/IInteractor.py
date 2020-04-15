#!/usr/bin/python
#coding: utf-8

""" System """

from sihd import Core

class IInteractor(Core.IPolyService, Core.IAppContainer):

    def __init__(self, app=None, name="IInteractor"):
        super(IInteractor, self).__init__(name)
        if (app):
            self.set_app(app)
        self.__interaction = None
        self.add_channel_input("interaction", type="queue")

    """ To change """

    def on_new_interaction(self, action):
        return action

    def interact(self, action=None, *args, **kwargs):
        if action is None:
            action = self.get_interaction()
        return self.on_interaction(action, *args, **kwargs)

    def on_interaction(self, action, *args, **kwargs):
        raise NotImplementedError("Interact not implemented")

    """ IInteractor """

    def set_app(self, app):
        super(IInteractor, self).set_app(app)
        app.add_interactor(self)

    def set_interaction(self, action):
        self.__interaction = self.on_new_interaction(action)

    def get_interaction(self):
        return self.__interaction

    """ IObserver """

    def on_notify(self, channel):
        if channel == self.interaction:
            action = channel.read()
            if action is not None:
                self.set_interaction(action)

    """ IProcessedService """

    def do_work(self, i):
        return self.do_step()

    """ Step method """

    def do_step(self):
        return self.interact()
