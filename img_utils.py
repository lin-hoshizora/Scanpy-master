"""
Utility functions for image processing
"""

import numpy as np
import cv2

def to_rgb(raw):
  """
  Convert YUV image to RGB
  :param raw: YUV image in raw bytes
  :return: RGB image in numpy array
  """
  w = 2048
  h = 1536
  U = np.ascontiguousarray(raw[0::4])
  Y1 = np.ascontiguousarray(raw[1::4])
  V = np.ascontiguousarray(raw[2::4])
  Y2 = np.ascontiguousarray(raw[3::4])
  UV = np.empty((w * h), dtype=np.uint8)
  YY = np.empty((w * h), dtype=np.uint8)
  UV[0::2] = np.frombuffer(U, dtype=np.uint8)
  UV[1::2] = np.frombuffer(V, dtype=np.uint8)
  YY[0::2] = np.frombuffer(Y1, dtype=np.uint8)
  YY[1::2] = np.frombuffer(Y2, dtype=np.uint8)
  UV = UV.reshape((h, w))
  YY = YY.reshape((h, w))
  yuv = cv2.merge([UV, YY])
  rgb = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB_YUY2)
  rgb_rotate = cv2.warpAffine(rgb, cv2.getRotationMatrix2D((w // 2, h // 2), 180, 1), (w, h))
  return rgb_rotate

def crop(img):
  """
  Crop out the largest rectangle area
  :param img: Original Image
  :return: The largest rectangle area
  """
  img_gray = cv2.cvtColor(img, cv2.COLOR_RGB2GRAY)
  img_blur = cv2.GaussianBlur(img_gray, (65, 65), 0)
  th, bi = cv2.threshold(img_blur, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
  contours, hierarchy = cv2.findContours(bi, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
  if len(contours) == 0:
    return img
  
  rotated = False
  large = sorted(contours, key=cv2.contourArea, reverse=True)[0]
  box = cv2.minAreaRect(large)
  pts = cv2.boxPoints(box)
  w, h = box[1][0], box[1][1]
  xs = [pt[0] for pt in pts]
  ys = [pt[1] for pt in pts]
  x1 = min(xs)
  x2 = max(xs)
  y1 = min(ys)
  y2 = max(ys)
  angle = box[2]
  if angle < -45:
    angle += 90
    rotated = True
  center = (int((x1+x2)/2), int((y1+y2)/2))
  size = (int(x2-x1), int(y2-y1))
  M = cv2.getRotationMatrix2D((size[0]/2, size[1]/2), angle, 1.0)
  crop = cv2.getRectSubPix(img, size, center)
  crop = cv2.warpAffine(crop, M, size)
  if rotated:
    w, h = h, w
  aligned = cv2.getRectSubPix(crop, (int(w), int(h)), (size[0]/2, size[1]/2))
  return aligned