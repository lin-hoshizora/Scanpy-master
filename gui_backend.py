import asyncio
import datetime
import sys
from pathlib import Path
import json
import subprocess
import websockets
import cv2
import scanpy
from img_utils import to_rgb, crop
import os
import yaml

# scanner
x50 = scanpy.X50()
sys.stdout.flush()

# settings
log_path = str(Path(__file__).parent/'backend_log.txt')

def log(lv, message):
  timestamp = datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
  text = ' '.join([timestamp, lv, message])
  with open(log_path, 'a') as f:
    f.write(text + '\n')
  print(text)
  sys.stdout.flush()

async def serve_scan(websocket, path):
  async for data in websocket:
    if not isinstance(data, str):
      log('WARN', 'ignoring non-string data: '+data)
      continue
    req = json.loads(data)
    if req['msg'] == 'start_scan':
      x50.warmup()
      img_full = to_rgb(x50.scan())
      sys.stdout.flush()

      save_dir_=Path(req['save'])/'not_crop'
      if not save_dir_.exists():
        os.makedirs(save_dir_)

      path_full = str(Path(req['save'])/'not_crop'/(req['id'] + '_クロッピングなし.jpg'))
      ret_full = cv2.imwrite(path_full, img_full[..., ::-1])
      img_crop = crop(img_full)
      path_crop = str(Path(req['save'])/req['id'])+'.jpg'
      ret_crop = cv2.imwrite(path_crop, img_crop[..., ::-1])
      res = {}
      res['id'] = req['id']
      res['success'] = (ret_full and ret_crop)
      res['full'] = path_full
      res['crop'] = path_crop
      msg = json.dumps(res)
      await websocket.send(msg)
    if req['msg'] == 'detect_paper':
      res = {}
      res['id'] = req['id']
      res['detected'] = (x50.detectPaperL() != 0 or x50.detectPaperR() != 0)
      sys.stdout.flush()
      msg = json.dumps(res)
      await websocket.send(msg)
    if req['msg'] == 'self_check':
      res = {}
      res['id'] = req['id']
      try:
        x50.warmup()
        img = to_rgb(x50.scan())
        if img.size > 500**2*3:
          res['check_passed'] = True
        else:
          res['check_passed'] = False
      except Exception as e:
        res['check_passed'] = False
        log('ERROR', 'caught exception: '+e)
      msg = json.dumps(res)
      await websocket.send(msg)
      

# start server
with open('port_config.yaml','r') as f:
  obj = yaml.safe_load(f)
port = obj['port']
print(type(port),port)
server_scan = websockets.serve(serve_scan, 'localhost', port)
asyncio.get_event_loop().run_until_complete(server_scan)
print('Server Up')
sys.stdout.flush()
asyncio.get_event_loop().run_forever()
