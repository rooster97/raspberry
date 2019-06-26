# -*- coding: utf-8 -*-
from __future__ import unicode_literals
from django.shortcuts import render
from datetime import datetime
import picamera, os
import numpy as np
import threading
from django.http import StreamingHttpResponse
from django.views.decorators import gzip
import cv2
import time
import sys

sys.path.append('/usr/local/lib/python2.7/site-packages')

# Create your views here.

class VideoCamera(object):
	def __init__(self):
		self.video = cv2.VideoCapture(0)
		(self.grabbed, self.frame) = self.video.read()
		threading.Thread(target=self.update, args=()).start()

	def __del__(self):
		self.video.release()
    
	def get_frame(self):
		image = self.frame
		ret, jpeg = cv2.imencode('.jpg', image)
		return jpeg.tobytes()

	def get_frame2(self):
		ret, image = self.video.read()
		ret, jpeg = cv2.imencode('.jpg', image)
		return jpeg.tobytes()

	def update(self):
		while True:
			(self.grabbed, self.frame) = self.video.read()

def gen(camera):
	while True:
		frame = camera.get_frame2()
		yield(b'--frame\r\n'
			b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n\r\n')

PATH = '/home/pi/raspberry/ipcamera/app/templates/resources/images/'

@gzip.gzip_page
def stream(request):
	try:
		return StreamingHttpResponse(gen(VideoCamera()), content_type="multipart/x-mixed-replace;boundary=frame")
	except:  # This is bad! replace it with proper handling
		pass

def live(request):
 return render(request, 'design/html/live.html')

def live_snapshot(request):
 now = datetime.now()
 fileName = PATH + now.strftime('%y%m%d_%H%M%S') + '.jpg' 
 cmd = 'raspistill -o ' + fileName
 print (cmd)
 os.system (cmd) 
 return render(request, 'design/html/live.html')

def playback(request):
 fileList = os.listdir(PATH)
 snapshotNames = []
 for fileName in fileList:
  if fileName[6] == '_': # Check snapshot. More verification is need.
   name = fileName[:13]  # Delete .jpg from file name
   snapshotNames.append(name)

 return render(request, 'design/html/playback.html', {'snapshotNames' : snapshotNames}) # should be 'snapshotNames' and snapshotNames be same ?

def setting(request):
 return render(request, 'design/html/setting.html')	
