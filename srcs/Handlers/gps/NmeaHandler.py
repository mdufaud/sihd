#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

pynmea2 = None

from sihd.srcs.Handlers.IHandler import IHandler

class NmeaHandler(IHandler):

    def __init__(self, app=None, name="NmeaHandler"):
        global pynmea
        if pynmea is None:
            import pynmea2
        self._gsv_init = False
        super(NmeaHandler, self).__init__(app=app, name=name)
        self._set_default_conf({
        })

    def __init_gsv_data(self):
        self._GsvMsgReceived = None
        self._GsvNbMsg = 0
        self._SV_PRN_num = [0] * 20
        self._SV_azimuth = [0] * 20
        self._SV_elevation = [0] * 20
        self._SV_SNR = [0] * 20
        self._gsv_init = True

    """ IConfigurable """

    def _load_conf_impl(self):
        return True

    """ Nmea """

    def _handle_gsv_msg(msg):
        if self._gsv_init is False:
            self.__init_gsv_data()

        _SV_PRN_num = self._SV_PRN_num
        _SV_azimuth = self._SV_azimuth
        _SV_elevation = self._SV_elevation
        _SV_SNR = self._SV_SNR

        MsgReceived = self._GsvMsgReceived

        Nb_SV_In_View = int(msg.num_sv_in_view)
        Num_CurMsg = int(msg.msg_num)

        offset = (Num_CurMsg - 1) * 4
        # Enregistrement des données dans les tableaux
        SV_PRN_num[offset + 0] = int(msg.sv_prn_num_1)
        SV_azimuth[offset + 0] = int(msg.azimuth_1)
        SV_elevation[offset + 0] = int(msg.elevation_deg_1)
        SV_SNR[offset + 0] = int(msg.snr_1)
        if offset + 2 <= Nb_SV_In_View:
            SV_PRN_num[offset + 1] = int(msg.sv_prn_num_2)
            SV_azimuth[offset + 1] = int(msg.azimuth_2)
            SV_elevation[offset + 1] = int(msg.elevation_deg_2)
            SV_SNR[offset + 1] = int(msg.snr_2)
        if offset + 3 <= Nb_SV_In_View:
            SV_PRN_num[offset + 2] = int(msg.sv_prn_num_3)
            SV_azimuth[offset + 2] = int(msg.azimuth_3)
            SV_elevation[offset + 2] = int(msg.elevation_deg_3)
            SV_SNR[offset + 2] = int(msg.snr_3)
        if offset + 4 <= Nb_SV_In_View:
            SV_PRN_num[offset + 3] = int(msg.sv_prn_num_4)
            SV_azimuth[offset + 3] = int(msg.azimuth_4)
            SV_elevation[offset + 3] = int(msg.elevation_deg_4)
            SV_SNR[offset + 3] = int(msg.snr_4)

        # On attend le message 1/N pour configurer le nb de messages à recevoir
        if Num_CurMsg == 1:
            NbMsg = int(msg.num_messages)
            self._GsvNbMsg = NbMsg
            self._GsvMsgReceived = [False] * NbMsg
            
        if not MsgReceived or self._GsvNbMsg <= 0:
            return
        MsgReceived[Num_CurMsg - 1] = True
        if all(MsgReceived):
            self._gsv_init = False
            dic = {
                "SV_PRN_num": SV_PRN_num[0:Nb_SV_In_View]
                "SV_azimuth_deg": SV_azimuth[0:Nb_SV_In_View]
                "SV_elevation": SV_elevation[0:Nb_SV_In_View]
                "SV_SNR": SV_SNR[0:Nb_SV_In_View]
                "SV_azimuth_rad": [x / 180.0*3.141593 for x in SV_azimuth_deg]
            }
            self.notify_observers(dic)
        return True

    """ IObservable """

    def handle(self, observable, line):
        if not self.is_active():
            return False
        s = line.decode('ascii', errors='replace').strip()
        try:
            msg = pynmea2.parse(s)
        except:
            return False
        ret = True
        if msg.sentence_type == 'GSV':
            ret = ret and self._handle_gsv_msg(msg)
        return ret

    """ IService """

    def _start_impl(self):
        return True

    def _stop_impl(self):
        return True

    def _pause_impl(self):
        return True

    def _resume_imp(self):
        return True
