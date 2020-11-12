import wx
import sihd

class MainWindow(wx.Frame):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._filemenu = [
            (wx.ID_ABOUT, '&About',
             sihd.string.get('wx.info_about', 'Information about this program'),
             self.on_about),
            (wx.ID_EXIT, '&Exit',
             sihd.strings.get('wx.info_exit', 'Terminate the program'),
             self.on_exit)
        ]

    def build_frames(self):
        self.build_interior_frame()
        self.build_exterior_frame()

    def build_interior_frame(self):
        pass

    def build_exterior_frame(self):
        self.create_menu()
        self.CreateStatusBar()

    def create_menu(self):
        fileMenu = wx.Menu()
        for id, label, helpText, handler in self._filemenu:
            if id == None:
                fileMenu.AppendSeparator()
            else:
                item = fileMenu.Append(id, label, helpText)
                self.Bind(wx.EVT_MENU, handler, item)
        menuBar = wx.MenuBar()
        menuBar.Append(fileMenu, '&File') # Add the fileMenu to the MenuBar
        self.SetMenuBar(menuBar)  # Add the menuBar to the Frame

    # Event handlers:

    def on_about(self, event):
        hdr = sihd.strings.get('wx.about_hdr', 'About')
        txt = sihd.strings.get('wx.about_txt', 'About program')
        dialog = wx.MessageDialog(self, hdr, txt, wx.OK)
        dialog.ShowModal()
        dialog.Destroy()

    def on_exit(self, event):
        self.Close()
