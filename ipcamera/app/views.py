# -*- coding: utf-8 -*-
from __future__ import unicode_literals
from django.http import HttpResponse
from django.template import loader
from django.shortcuts import render
from django.utils import timezone
import numpy as np
import threading
from django.http import StreamingHttpResponse
from datetime import datetime
import os
import cv2
from app.models import Image


# Create your views here.
 
directory = os.getcwd()
filePath = directory + '/app/templates/resources/images/'

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

	def take_frame(self):
		now = datetime.now()
		fileName = filePath + now.strftime('%y%m%d_%H%M%S') + '.png'
		print (fileName)
		cv2.imwrite(fileName, self.frame)

		db = Image(image_name=now.strftime('%y%m%d_%H%M%S'), pub_date=timezone.now())
		db.save()

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
		cam.take_frame()

	return render(request, 'design/html/live.html')

def playback(request):
	image_list = Image.objects.all()
	return render(request, 'design/html/playback.html',
							{'image_list' : image_list})

def playback_show(request, select_image):
	image_list = Image.objects.all()
	return render(request, 'design/html/playback.html',
							{'image_list' : image_list,  'select_image' : select_image})
def setting(request):
 return render(request, 'design/html/setting.html')
