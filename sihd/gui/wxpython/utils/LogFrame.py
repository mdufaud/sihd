import wx
import locale
import logging
import sihd

class LogFrame(wx.Panel):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._log_handler = None
        self.max_lines = 5
        self.create_controls()
        self.bind_events()
        self.do_layout()

    def create_controls(self):
        self.logger_ctrl = wx.TextCtrl(self, size=(800, 400),
                                        #style=wx.TE_MULTILINE | wx.TE_READONLY)
                                        style=wx.TE_MULTILINE)

    def bind_events(self):
        self.logger_ctrl.Bind(wx.EVT_TEXT, self.on_text_entered)

    def on_text_entered(self, event):
        '''
            Input Only
            Parse text with: text = event.GetString()
        '''
        pass

    def do_layout(self):
        boxSizer = wx.BoxSizer(orient=wx.HORIZONTAL)
        boxSizer.Add(self.logger_ctrl)
        self.SetSizerAndFit(boxSizer)

    def erase_append(self, ctrl, nlines, new_msg):
        size = 0
        for i in range(0, nlines):
            size += ctrl.GetLineLength(i) + 1
        txt = ctrl.GetValue()
        ctrl.SetValue(txt[size:] + new_msg)

    def log(self, message):
        wx.CallAfter(self._do_log, message)

    def _do_log(self, message):
        ctrl = self.logger_ctrl
        n = ctrl.GetNumberOfLines()
        diff = n - self.max_lines
        if diff >= 0:
            self.erase_append(ctrl, diff + 1, message)
        else:
            ctrl.AppendText('%s' % message)

    def add_handler(self):
        # Add a logging handler
        if self._log_handler is None:
            log_handler = WxLogHandler(self)
            log_handler.setFormatter(sihd.log.get_stream_formatter())
            sihd.log.logger.addHandler(log_handler)
            self._log_handler = log_handler

    def remove_handler(self):
        # Removes a logging handler
        handler = self._log_handler
        if handler is not None:
            sihd.log.logger.removeHandler(handler)
            self._log_handler = None

#
# Logging handler
#

try:
    unicode
    _unicode = True
except NameError:
    _unicode = False

class WxLogHandler(logging.Handler):

    def __init__(self, frame):
        logging.Handler.__init__(self)
        self.frame = frame
        self.code = locale.getpreferredencoding()

    def emit(self, record):
        try:
            msg = self.format(record)
            fs = "\n%s"
            to_print = None
            if not _unicode: #if no unicode support...
                to_print = fs % msg
            else:
                try:
                    if (isinstance(msg, unicode)):
                        ufs = u'\n%s'
                        try:
                            to_print = ufs % msg
                        except UnicodeEncodeError:
                            to_print = (ufs % msg).encode(self.code)
                    else:
                        to_print = fs % msg
                except UnicodeError:
                    to_print = fs % msg.encode("UTF-8")
            self.frame.log(to_print)
        except (KeyboardInterrupt, SystemExit):
            raise
        except:
            self.handleError(record)
