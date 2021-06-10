# -*- coding: utf-8 -*-

"""
GUI for Plustek X50
"""

import sys
import atexit
from os.path import expanduser
import datetime
from pathlib import Path
import json
import subprocess
from PyQt5.QtWidgets import QApplication, QWidget, QPushButton, QFileSystemModel, QTreeView, QMessageBox, QSpinBox
from PyQt5.QtWidgets import QVBoxLayout, QHBoxLayout, QLabel, QTabWidget, QFileDialog, QLineEdit, QCheckBox
from PyQt5.QtGui import QFont, QPixmap
from PyQt5 import QtCore, QtWebSockets
from PyQt5.QtCore import QTimer, QUrl

import yaml


DET_PERIOD = 200
TIMEOUT = 8000

class Scanner(QWidget):
  def __init__(self):
    super().__init__()
    self.backend = None
    self.log_path = str(Path(__file__).parent/'gui_log.txt')
    self.wait_manual = False
    self.paper_detected = False
    self.n_pass = 0
    self.crop_path = None
    self.full_path = None
    self.init_gui()
    self.init_timer()
    self.boot_backend()
    while True:
      self.backend.stdout.flush()
      nextline = self.backend.stdout.readline()
      if "Server Up" in nextline:
        break
    # time.sleep(1)
    with open('port_config.yaml','r') as f:
      obj = yaml.safe_load(f)
    self.port = str(obj['port'])
    self.init_ws()

  def log(self, message, lv='INFO'):
    timestamp = datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
    text = ' '.join([timestamp, lv, message])
    with open(self.log_path, 'a') as f:
      f.write(text + '\n')
    print(text)

  def boot_backend(self):
    is_reboot = (self.backend is not None)
    if is_reboot:
      self.auto_scan_check.setChecked(True)
      if self.n_pass == 0:
        self.img_show.setText('スキャナー自動再接続中。。。')
        self.img_show.repaint()
      self.backend.kill()
      self.req_timer.stop()
      self.log('killed existing backend')
      # time.sleep(10)
    cmd = ['python3', str(Path(__file__).parent/'gui_backend.py')]
    self.backend = subprocess.Popen(cmd, stdout=subprocess.PIPE, universal_newlines=True)
    self.log('started new backend')
    if is_reboot:
      while True:
        self.backend.stdout.flush()
        nextline = self.backend.stdout.readline()
        # print(nextline)
        if "Server Up" in nextline:
          break
      # time.sleep(1)
      self.ws_client.open(QUrl('ws://localhost:'+str(self.port)))

  def timed_out(self):
    self.log('request timed out, rebooting backend', 'ERROR')
    self.n_pass = 0
    self.boot_backend()

  def init_ws(self):
    self.ws_client = QtWebSockets.QWebSocket()
    self.ws_client.textMessageReceived.connect(self.ws_message)
    self.ws_client.error.connect(self.ws_error)
    self.ws_client.connected.connect(self.ws_connected)
    self.ws_client.open(QUrl('ws://localhost:'+str(self.port)))

  def set_det_timer(self):
    if self.auto_scan_check.isChecked():
      self.det_timer.start(DET_PERIOD)
    else:
      self.det_timer.stop()

  def ws_connected(self):
    self.log('websocket connected')
    self.send_req('self_check')

  def ws_error(self, error_code):
    self.log(f'ERROR CODE: {error_code} {self.ws_client.errorString()}')

  def ws_message(self, message):
    res = json.loads(message)
    # print(message)
    if 'crop' in res:
      # scan result
      self.req_timer.stop()
      if res['success']:
        self.crop_path = res['crop']
        self.full_path = res['full']
        self.update_img()
      else:
        self.log('scan failed', 'ERROR')
        err_msg = f'スキャン画像を{str(self.path)}保存できませんでした。'
        QMessageBox.information(self, 'エラー', err_msg)
        self.img_show.clear()
      self.wait_manual = False
    elif 'detected' in res:
      if res['detected'] and not self.paper_detected:
        self.manual_scan()
      else:
        self.det_timer.start(DET_PERIOD)
      self.paper_detected = res['detected']
    elif 'check_passed' in res:
      if res['check_passed']:
        self.n_pass += 1
        if self.n_pass < 2:
          self.boot_backend()
        else:
          self.n_pass = 0
          if self.wait_manual:
            # recover from scan
            self.wait_manual = False
            self.paper_detected = True
            self.manual_scan()
          else:
            # start detection
            self.img_show.setText('')
            self.img_show.repaint()
            self.paper_detected = False
      else:
        # self check failed
        self.n_pass = 0
        self.boot_backend()
    else:
      self.log('invalid response: '+message, 'ERROR')
    # recover autoscan
    self.set_det_timer()

  def init_timer(self):
    # timer for paper detection
    self.det_timer = QTimer(self)
    self.det_timer.timeout.connect(self.detect_paper)

    # timer for wait response
    self.req_timer = QTimer(self)
    self.req_timer.timeout.connect(self.timed_out)

    # timer for clear the image shown on the screen
    self.clear_timer = QTimer(self)
    self.clear_timer.timeout.connect(self.onClear)

  def init_gui(self):
    """
    Custom GUI
    """
    self.font = QFont()
    self.font.setFamily('Noto Sans CJK JP')
    self.font.setPointSize(30)
    self.tab_font = QFont()
    self.tab_font.setPointSize(20)
    self.tab_font.setFamily('Noto Sans CJK JP')

    self.setWindowTitle('アルメックス証明書スキャナー')

    self.scan_button = QPushButton('手動スタート')
    self.scan_button.setFont(self.font)
    self.scan_button.setMinimumHeight(100)
    self.scan_button.setMaximumWidth(300)
    self.scan_button.clicked.connect(self.manual_scan)

    self.reconn_button = QPushButton('再接続')
    self.reconn_button.setFont(self.font)
    self.reconn_button.setMinimumHeight(100)
    self.reconn_button.setMaximumWidth(300)
    self.reconn_button.clicked.connect(self.boot_backend)

    self.quit_button = QPushButton('プログラム終了')
    self.quit_button.setFont(self.font)
    self.quit_button.clicked.connect(self.close)
    self.quit_button.setMinimumHeight(100)
    self.quit_button.setMaximumWidth(300)
    
    self.img_tab = QWidget()
    self.img_layout = QVBoxLayout()
    self.img_title = QLabel()
    self.img_title.setFont(self.tab_font)
    self.img_show = QLabel('スキャン結果')
    self.img_show.setFont(self.font)
    self.img_show.setMinimumSize(800, 450)
    self.img_show.setAlignment(QtCore.Qt.AlignCenter)
    self.crop_check = QCheckBox('自動クロッピング後の画像を表示')
    self.crop_check.setChecked(True)
    self.crop_check.stateChanged.connect(self.set_crop)
    self.crop_check.setStyleSheet('QCheckBox::indicator {width: 25px; height: 25px}')
    self.crop_check.setFont(self.tab_font)
    self.img_layout.addWidget(self.img_show)
    self.img_layout.addWidget(self.img_title)
    self.img_layout.addWidget(self.crop_check)
    self.img_layout.setAlignment(QtCore.Qt.AlignCenter)
    self.img_tab.setLayout(self.img_layout)

    self.help_tab = QLabel()
    self.help_tab.setAlignment(QtCore.Qt.AlignTop)
    # with open('/home/wangg/Scanpy/gui_descript.txt') as f:
    with open('./gui_descript.txt') as f:
      self.help_tab.setText(f.read())


    self.setting_tab = QWidget()
    # auto scan
    self.auto_scan_check = QCheckBox('紙が検出された時自動スキャン')
    self.auto_scan_check.setChecked(True)
    self.auto_scan_check.stateChanged.connect(self.set_auto)
    self.auto_scan_check.setStyleSheet('QCheckBox::indicator {width: 25px; height: 25px}')
    self.auto_scan_check.setFont(self.tab_font)

    # save path
    self.path_title = QLabel('保存フォルダー')
    self.path = Path(expanduser('~'))/'ピクチャ'
    self.path_textedit = QLineEdit()
    self.path_textedit.setText(str(self.path))
    self.path_textedit.textChanged.connect(self.change_view)
    # auto clear
    self.showtime_area = QWidget()
    self.showtime_title = QLabel('スキャン画像表示時間(秒)')
    self.showtime_title.setFont(self.tab_font)
    self.showtime = 5
    self.showtime_spinbox = QSpinBox()
    self.showtime_spinbox.setValue(self.showtime)
    self.showtime_spinbox.valueChanged.connect(self.onShowTimeSet)
    self.showtime_spinbox.setMinimumHeight(55)
    self.showtime_spinbox.setFont(self.tab_font)
    self.showtime_layout = QHBoxLayout()
    self.showtime_layout.addWidget(self.showtime_title)
    self.showtime_layout.addWidget(self.showtime_spinbox)
    self.showtime_layout.setAlignment(QtCore.Qt.AlignLeft)
    self.showtime_area.setLayout(self.showtime_layout)
    self.showtime_area.setStyleSheet("QSpinBox::up-button { height: 25px; width: 25px} QSpinBox::down-button { height: 25px; width: 25px}")
    

    self.path_button = QPushButton('保存フォルダー変更')
    self.path_button.clicked.connect(self.select_folder)
    self.path_setting = QWidget()
    self.path_layout = QHBoxLayout()
    self.path_layout.addWidget(self.path_textedit)
    self.path_layout.addWidget(self.path_button)
    self.path_setting.setLayout(self.path_layout)
    # folder viewer
    self.tree_title = QLabel('ファイル一覧')
    # self.tree_title.setFont(self.font)
    self.model = QFileSystemModel()
    self.model.setRootPath(expanduser('~'))
    self.tree = QTreeView()
    self.tree.setModel(self.model)
    self.tree.setRootIndex(self.model.index(str(self.path)))
    self.tree.setAnimated(False)
    self.tree.setIndentation(20)
    self.tree.setSortingEnabled(True)
    self.settings_layout = QVBoxLayout()
    self.settings_layout.setAlignment(QtCore.Qt.AlignTop)
    self.settings_layout.addWidget(self.auto_scan_check)
    self.settings_layout.addWidget(self.showtime_area)
    self.settings_layout.addWidget(self.path_title)
    self.settings_layout.addWidget(self.path_setting)
    self.settings_layout.addWidget(self.tree_title)
    self.settings_layout.addWidget(self.tree)
    self.setting_tab.setLayout(self.settings_layout)

    self.tabs = QTabWidget()
    self.tabs.addTab(self.img_tab, 'スキャン画像')
    self.tabs.addTab(self.setting_tab, '設定')
    self.tabs.addTab(self.help_tab, '説明')
    self.tabs.setFont(self.tab_font)
    
    self.buttons = QWidget()
    self.button_layout = QHBoxLayout()
    self.button_layout.addWidget(self.scan_button)
    self.button_layout.addWidget(self.reconn_button)
    self.button_layout.addWidget(self.quit_button)
    self.buttons.setLayout(self.button_layout)

    self.stack = QVBoxLayout()
    self.stack.addWidget(self.tabs)
    self.stack.addWidget(self.buttons)
    self.setLayout(self.stack)
    self.showFullScreen()

  def update_img(self):
    """
    Update scanned image
    """
    if self.crop_check.isChecked():
      pixmap = QPixmap(str(self.crop_path))
    else:
      pixmap = QPixmap(str(self.full_path))
    if pixmap.width() / pixmap.height() > self.img_show.width() / self.img_show.height():
      self.img_show.setPixmap(pixmap.scaledToWidth(self.img_show.width()))
    else:
      self.img_show.setPixmap(pixmap.scaledToHeight(self.img_show.height()))
    self.img_title.setText('画像を' + str(self.crop_path) + 'に保存しました。')
    self.img_title.setVisible(True)
    self.crop_check.setVisible(True)
    self.clear_timer.setSingleShot(True)
    self.clear_timer.start(self.showtime*1000)
    

  def select_folder(self):
    """
    Select save folder from a dialogue
    """
    path = QFileDialog.getExistingDirectory(None, '保存パスを選んでください', str(self.path))
    self.path = Path(path)
    self.path_textedit.setText(path)

  def change_view(self):
    """
    Update the file viewer
    """
    self.tree.setRootIndex(self.model.index(self.path_textedit.text()))

  def send_req(self, message):
    req_dic = {}
    timestamp = datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
    req_dic['id'] = timestamp
    req_dic['msg'] = message
    if message == 'start_scan':
      req_dic['save'] = str(self.path)
      self.wait_manual = True
    req = json.dumps(req_dic)
    self.ws_client.sendTextMessage(req)
    self.req_timer.setSingleShot(True)
    self.req_timer.start(TIMEOUT)
    if message == 'self_check':
      self.img_show.setText('キャリブレーション中\n少々お待ちください')
      self.img_show.repaint()
    return req

  def set_auto(self):
    if self.auto_scan_check.isChecked():
      self.det_timer.start(DET_PERIOD)
    else:
      self.det_timer.stop()

  def set_crop(self):
    if self.clear_timer.isActive():
      self.clear_timer.stop()
      self.update_img()
  
  def detect_paper(self):
    # manual mode overrides auto mode
    if not self.wait_manual:
      self.send_req('detect_paper')
      self.det_timer.stop()

  def manual_scan(self):
    if self.wait_manual:
      self.log('Button clicking ignored when waiting for a scan result')
      return
    self.img_title.setVisible(False)
    self.crop_check.setVisible(False)
    self.clear_timer.stop()
    self.det_timer.stop()
    self.send_req('start_scan')
    self.show_scan_msg()

  def show_scan_msg(self):
    self.img_show.setText('スキャン中。。。')
    self.img_title.setText('')
    self.img_show.repaint()
    self.img_title.repaint()

  def onShowTimeSet(self):
    self.showtime = self.showtime_spinbox.value()

  def onClear(self):
    self.img_show.clear()
    self.img_show.setText(f'個人情報保護のため、\n画像表示時間は{self.showtime}秒に設定されています。')

def kill_backend():
  if scanner.ws_client:
    scanner.ws_client.close()
  if scanner.backend:
    scanner.backend.kill()
    scanner.log('killed backend before exiting')

if __name__ == '__main__':
  app = QApplication([])
  app.setStyle('Fusion')
  scanner = Scanner()
  atexit.register(kill_backend)
  sys.exit(app.exec_())
