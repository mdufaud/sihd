#!/usr/bin/python
#coding: utf-8

from sihd.Core.SihdRunnableService import SihdRunnableService
from sihd.Core.IAppContainer import IAppContainer

class AInteractor(SihdRunnableService, IAppContainer):

    def __init__(self, app=None, name="AInteractor", **kwargs):
        IAppContainer.__init__(self)
        super().__init__(name, **kwargs)
        self.__interaction = None
        self.add_channel_input("new_interaction", type="queue")
        self.add_channel_output("result")
        if app:
            self.set_app(app)

    """ To implement """

    def on_new_interaction(self, action: any) -> any:
        """ Returns parsed interacton from interaction channel """
        return action

    def do_interaction(self, action, *args, **kwargs) -> bool:
        raise NotImplementedError("do_interaction not implemented")

    """ AInteractor """

    def interact(self, action=None, *args, **kwargs) -> bool:
        if action is None:
            action = self.get_interaction()
        return self.do_interaction(action, *args, **kwargs)

    def set_interaction(self, action):
        self.__interaction = self.on_new_interaction(action)

    def get_interaction(self):
        return self.__interaction

    def set_result(self, res):
        self.result.write(res)

    """ IAppContainer """

    def set_app(self, app):
        super().set_app(app)
        self.set_namedobject_parent(app)
        app.add_interactor(self)

    """ SihdService """

    def _pre_handle(self, channel):
        ret = super()._pre_handle(channel)
        if not ret:
            if channel == self.new_interaction:
                action = channel.read()
                if action is not None:
                    self.set_interaction(action)
                ret = True
        return ret

    """ Step method """

    def on_step(self):
        return self.interact()
