# -*- coding: utf-8 -*-
from __future__ import unicode_literals
from django.shortcuts import render
from datetime import datetime
import picamera, os

# Create your views here.
 
PATH = '/home/pi/raspberry/ipcamera/app/templates/resources/images/'

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
