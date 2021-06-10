import time
from pathlib import Path
import datetime
import cv2
import numpy as np
import scanpy
from img_utils import to_rgb, crop

if __name__ == "__main__":
  scanner = scanpy.X50()
  while True:
    cmd = input()
    if cmd == 'q':
      break
    scanner.warmup()
    img = to_rgb(scanner.scan())
    img_crop = crop(img)
    if max(img_crop.shape) > 1550:
      img_crop = img
    cv2.imwrite(f"./imgs/{datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')}.jpg", img_crop[:, :, ::-1])
  scanner.clean()
