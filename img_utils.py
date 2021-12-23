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
  #bg = cv2.imread('./scanner_bg.jpg')
  #bg_gray = cv2.cvtColor(bg, cv2.COLOR_BGR2GRAY)
  #bg_blur = cv2.GaussianBlur(bg_gray, (65, 65), 0)
  #diff = np.clip(img_blur.astype(np.float32) - bg_blur.astype(np.float32), a_min=0, a_max=255)
  #diff = diff.astype(np.uint8)
  th, bi = cv2.threshold(img_blur, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
  contours, hierarchy = cv2.findContours(bi, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
  if len(contours) == 0:
    return cv2.rotate(img,cv2.ROTATE_90_COUNTERCLOCKWISE)

  rotated = False
  large = sorted(contours, key=cv2.contourArea, reverse=True)[0]
  box = cv2.minAreaRect(large)
  pts = cv2.boxPoints(box)
  w, h = box[1]
  angle = box[2]
  if angle < -45:
    angle += 90
    rotated = True
  if rotated:
    w, h = h, w

  side01 = np.sqrt(((pts[0] - pts[1])**2).sum())
  side12 = np.sqrt(((pts[1] - pts[2])**2).sum())
  if side01 > side12:
    short_sides = [sorted([pts[1], pts[2]], key=lambda x: x[1]), sorted([pts[0], pts[3]], key=lambda x: x[1])]
    short_length, long_length = side12, side01
  else:
    short_sides = [sorted([pts[0], pts[1]], key=lambda x: x[1]), sorted([pts[3], pts[2]], key=lambda x: x[1])]
    short_length, long_length = side01, side12
  if short_length < 888 and 1000 < long_length < 1600:
    dl = (900 - short_length)
    for idx, _ in enumerate(short_sides):
      pt1, pt2 = short_sides[idx]
      side_angle = np.arctan((pt2[1] - pt1[1]) / (pt2[0] - pt1[0] + 1e-9))
      dx = np.cos(side_angle) * dl
      dy = np.sin(side_angle) * dl
      if side_angle < 0:
        dx = -dx
        dy = -dy
      pt1[0] = max(pt1[0] - dx, 0)
      pt1[1] = max(pt1[1] - dy, 0)
      short_sides[idx][0] = pt1
    pts = np.array(short_sides).reshape(-1, 2)
    h += dl

  xs = [pt[0] for pt in pts]
  ys = [pt[1] for pt in pts]
  x1 = min(xs)
  x2 = max(xs)
  y1 = min(ys)
  y2 = max(ys)
  center = (int((x1+x2)/2), int((y1+y2)/2))
  size = (int(x2-x1), int(y2-y1))
  M = cv2.getRotationMatrix2D((size[0]/2, size[1]/2), angle, 1.0)
  crop = cv2.getRectSubPix(img, size, center)
  crop = cv2.warpAffine(crop, M, size)
  aligned = cv2.getRectSubPix(crop, (int(w), int(h)), (size[0]/2, size[1]/2))
  new_long = max(aligned.shape[:2])

  if max(w,h) > 1800:
    return cv2.rotate(img,cv2.ROTATE_90_COUNTERCLOCKWISE)

  print(aligned.shape)
  if new_long > 1600 or (aligned.shape[0] > 1200 and aligned.shape[1] > 1200):
    return cv2.rotate(img,cv2.ROTATE_90_COUNTERCLOCKWISE)

  if aligned.shape[0] <800 or aligned.shape[1] < 1300:
    return img[0:900,600:]
  return aligned