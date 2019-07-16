# -*- coding: utf-8 -*-
from __future__ import unicode_literals
from django.http import HttpResponse
from django.template import loader
from django.shortcuts import render
import numpy as np
import threading
from django.http import StreamingHttpResponse
from datetime import datetime
import os
import cv2

# Create your views here.
 
PATH = '/home/pi/raspberry/ipcamera/app/templates/resources/images/'

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

	def update(self):
		while True:
			(self.grabbed, self.frame) = self.video.read()

cam = VideoCamera()

def gen(camera):
	while True:
		frame = cam.get_frame()
		yield(b'--frame\r\n'
			b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n\r\n')

def stream2(request):
    try:
        return StreamingHttpResponse(gen(()), content_type="multipart/x-mixed-replace;boundary=frame")
    except:  # This is bad! replace it with proper handling
        pass

def live(request):
	if request.method == 'POST':
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
