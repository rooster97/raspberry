# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.shortcuts import render

# Create your views here.

def live(request):
 return render(request, 'design/html/live.html')

def playback(request):
 return render(request, 'design/html/playback.html')

def setting(request):
 return render(request, 'design/html/setting.html')	
