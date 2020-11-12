import wx

#
# CallAfter
#

def post(func, data):
    # Delay func call after event finishes - Essential in threading
    wx.CallAfter(func, data)

#
# Id - Connect - PostEvent
#

def new_id():
    # Generates an ID
    return wx.NewId()

class ResultEvent(wx.PyEvent):

    # Creates an event to be given to PostEvent

    def __init__(self, evt_id, data):
        wx.PyEvent.__init__(self)
        self.SetEventType(evt_id)
        self.data = data

class EventConnector(object):

    def __init__(self):
        self.id = wx.NewId()
        self.evt_handlers = {}

    def connect(self, evt_handler, func):
        # Connects a Wx EvtHandler object to an event and a callback method
        if evt_handler in self.evt_handlers:
            raise KeyError("EvtHandler already connected")
            self.evt_handlers[evt_handler] = func
            evt_handler.Connect(-1, -1, self.id, func)
            return True
        return False

    def disconnect(self, evt_handler):
        func = self.evt_handlers[evt_handler]
        evt_handler.Disconnect(-1, -1, self.id, func)
        del self.evt_handlers[evt_handler]

    def post(self, data, evt_handler=None):
        # Post an event to either all evt_handlers or just one
        evt_handlers = None
        if evt_handler is not None:
            evt_handlers = [evt_handler]
        else:
            evt_handlers = self.evt_handlers.keys()
        evt = ResultEvent(self.id, data)
        for evt_handler in evt_handlers:
            wx.PostEvent(evt_handler, evt)
